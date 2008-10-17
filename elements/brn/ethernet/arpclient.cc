/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * arpclient.{cc,hh}
 */

#include <click/config.h>
#include "elements/brn/common.hh"
#include "arpclient.hh"
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>


CLICK_DECLS

ARPClient::ARPClient() :
  _debug(BrnLogger::DEFAULT),
  _request_timer(this)
{
}

ARPClient::~ARPClient()
{
}

int
ARPClient::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String router_ip;

  _active = true;
  if (cp_va_kparse(conf, this, errh,
    cpOptional,
      cpIPAddress, "Client IP address", &_client_ip,
      cpEthernetAddress, "Client ethernet address", &_client_ethernet,
      cpIPAddress, "Requested IP address Start", &_start_range,
      cpInteger, "Requested Address _range", &_range,
      cpInteger, "Start", &_client_start,
      cpInteger, "Interval", &_client_interval,
      cpInteger, "_count", &_count,
      cpInteger, "Requests at once", &_requests_at_once,
      cpInteger, "_timeout in ms", &_timeout,
    cpKeywords,
      "ACTIVE", cpBool, "ACTIVE", &_active,
      "DEBUG",  cpInteger, "DEBUG", &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
ARPClient::initialize(ErrorHandler *)
{
  _range_index = 0;

  _count_request = 0;
  _count_reply = 0;
  _count_timeout = 0;

  BRN_INFO("ReqIP: %s _range: %d Start: %d Interval: %d",_start_range.unparse().c_str(),
    _range,_client_start,_client_interval);

  _request_timer.initialize(this);
  if (_active)
    _request_timer.schedule_after_msec(_client_start);

  return 0;
}

void 
ARPClient::cleanup(CleanupStage stage)
{
  if (CLEANUP_ROUTER_INITIALIZED != stage)
    return;

  BRN_INFO("ARPClient: Results");
  BRN_INFO("ARPClient: Requests: %d",_count_request);
  BRN_INFO("ARPClient: Replies: %d",_count_reply);
  BRN_INFO("ARPClient: Timeouts: %d",_count_timeout);
  BRN_INFO("ARPClient: Undefined: %d",_request_queue.size());

  BRN_DEBUG("ARPClient:Skript%s %d %d %d %d",_client_ip.unparse().c_str(),
    _count_request,_count_reply,_count_timeout,_request_queue.size());

  _request_queue.clear();
}

void
ARPClient::run_timer()
{
  long time_diff;
  //BRN_DEBUG("ARPClient: Timer");
  struct timeval _time_now;
  _time_now = Timestamp::now().timeval();

  int i;

  for ( i = 0; i < _request_queue.size(); i++ )
  {
    time_diff = diff_in_ms(_time_now, _request_queue[i]._time_start );
    if ( time_diff > _timeout )
    {
       _count_timeout++;
       BRN_INFO("ARPClient:_timeout for IP: %s",_request_queue[i].ip_add.unparse().c_str());
       BRN_INFO("ARPClient: TIME: %d", time_diff);
       _request_queue.erase( _request_queue.begin() + i );
    }
  }
  
  if ( ! ( ( _count > 0 ) || ( _count == -1 ) ) )  //wenn ich nicht mehr nach IPs fargen muss dann raus
  {
    BRN_DEBUG("ARPClient:Skript%s %d %d %d %d",_client_ip.unparse().c_str(),
      _count_request,_count_reply,_count_timeout,_request_queue.size());
    _request_timer.schedule_after_msec(_client_interval);
    return;
  }
  

  if ( _request_queue.size() < _requests_at_once )  //kann ich noch anfragen machen
  {
    //_range_index = ( _range_index + 1 ) % _range; // sonst einfach ein s hoch aber jetzt
    BRN_DEBUG("ARPClient:Alles zufall");
    _range_index = click_random() % _range ;
    
    int requested_ip = ntohl(_start_range.addr()) + _range_index;
    
    // do not ask for own ip, simply use the next one
    if (requested_ip == ntohl(_client_ip.addr()))
      requested_ip++;
    
    requested_ip = htonl(requested_ip);

    IPAddress ip_add(requested_ip);

  
    for ( i = 0; i < _request_queue.size(); i++ )
    {
      if ( _request_queue[i].ip_add == ip_add )
        break;
    }

    if ( i != _request_queue.size() )
    {
      BRN_DEBUG("ARPClient: Nach IP: %s wird schon gefragt",ip_add.unparse().c_str());
    } 
    else
    {
      BRN_DEBUG("ARPClient: ich frage nach IP: %s",ip_add.unparse().c_str());
      _request_queue.push_back( ARPClientRequest(requested_ip) );
      send_arp_request( ip_add.data() );

      if (_count > 0 ) _count--;
    }
  }

  BRN_DEBUG("ARPClient:Skript%s %d %d %d %d",_client_ip.unparse().c_str(),
    _count_request,_count_reply,_count_timeout,_request_queue.size());
  _request_timer.schedule_after_msec(_client_interval);
 
}

void
ARPClient::push( int port, Packet *packet )  
{
  int result;

  BRN_DEBUG("ARPClient: PUSH an port: %d\n",port);

  if ( port == 0 )
    result = arp_reply(packet);

  BRN_DEBUG("ARPClient:Skript%s %d %d %d %d",_client_ip.unparse().c_str(),
    _count_request,_count_reply,_count_timeout,_request_queue.size());
}


/* Method processes an incoming packet. */
int
ARPClient::arp_reply(Packet *p)
{
  long time_diff;

  BRN_DEBUG("ARPClient: Packet\n");

  struct timeval _time_now;
  _time_now = Timestamp::now().timeval();

  click_ether *ethp = (click_ether *) p->data();                  //etherhaeder rausholen
  click_ether_arp *arpp = (click_ether_arp *) (ethp + 1);         //arpheader rausholen

  if ( ntohs(arpp->ea_hdr.ar_op) == ARPOP_REPLY )
  {
    BRN_DEBUG("ARPClient: Packet ist ARP-Reply\n");

    IPAddress ip_add(arpp->arp_spa);
    EtherAddress eth_add(arpp->arp_sha);

    for ( int i = 0; i < _request_queue.size(); i++ )
    {
      if ( _request_queue[i].ip_add == ip_add )
      {
        time_diff = diff_in_ms(_time_now, _request_queue[i]._time_start );
        BRN_INFO("ARPClient: TIME: %d",time_diff);
        _count_reply++;
        _request_queue.erase( _request_queue.begin() + i );
        break;
      }
    }

    BRN_INFO("ARPClient: IP: %s  MAC: %s\n",ip_add.unparse().c_str(),eth_add.unparse().c_str());
  }  
  else
  {
    BRN_ERROR("ARPClient: Error\n");
  }

  p->kill();

  return 0;
}


int
ARPClient::send_arp_request( uint8_t *d_ip_add )
{
  click_ether *e;
  click_ether_arp *ea;
		
  WritablePacket *p = Packet::make(sizeof(click_ether)+sizeof(click_ether_arp));      //neues Paket

  p->set_mac_header(p->data(), 14);
  memset(p->data(), '\0', p->length());

  e = (click_ether *) p->data();	                                 //pointer auf Ether-header holen
  ea = (click_ether_arp *) (e + 1);                                //Pointer auf ARPClient-Header holen
	
  uint8_t bcast_add[] = { 255,255,255,255,255,255 };

  memcpy(e->ether_dhost, bcast_add, 6);                             //alte Quelle ist neues Ziel des Etherframes
  memcpy(e->ether_shost, _client_ethernet.data(), 6);
		
  e->ether_type = htons(ETHERTYPE_ARP);                          //type im Ether-Header auf ARP setzten
		
  //Felder von ARP fllen
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);                       //Type der Hardware Adresse setzen
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);                       //Type der Protokoll Adresse setzten
  ea->ea_hdr.ar_hln = 6;                                         //L�ge setzten
  ea->ea_hdr.ar_pln = 4;                                         //L�ge
  ea->ea_hdr.ar_op = htons(ARPOP_REQUEST);                         //arp_opcode auf Request setzen
		
  //neues Packet basteln, erstmal quell und target zeug vertauschen
  memcpy(ea->arp_tpa, d_ip_add, 4);                         //neues Target ist altes Source
  memcpy(ea->arp_sha, _client_ethernet.data(), 6);                         //neues Source der Tabelle entnehmen
  memcpy(ea->arp_spa, _client_ip.data(), 4);                         //neues Source der Tabelle entnehmen
		
  BRN_DEBUG("ARPClient: send ARP-Request");

  _count_request++;
  output( 0 ).push( p );
 
  return(0);

}

long
ARPClient::diff_in_ms(timeval t1, timeval t2)
{
  long s, us, ms;

  while (t1.tv_usec < t2.tv_usec) {
    t1.tv_usec += 1000000;
    t1.tv_sec -= 1;
  }

  s = t1.tv_sec - t2.tv_sec;

  us = t1.tv_usec - t2.tv_usec;
  ms = s * 1000L + us / 1000L;

  return ms;
}

enum {
  H_DEBUG, 
  H_CLIENT_IP, 
  H_CLIENT_ETHERNET,
  H_START_RANGE,
  H_RANGE,
  H_CLIENT_START,
  H_CLIENT_INTERVAL,
  H_COUNT,
  H_REQUESTS_AT_ONCE,
  H_TIMEOUT,
  H_ACTIVE
};

static String 
read_param(Element *e, void *thunk)
{
  ARPClient *td = (ARPClient *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_CLIENT_IP:
    return td->_client_ip.unparse() + "\n";
  case H_CLIENT_ETHERNET:
    return td->_client_ethernet.unparse() + "\n";
  case H_START_RANGE:
    return td->_start_range.unparse() + "\n";
  case H_RANGE:
    return String(td->_range) + "\n";
  case H_CLIENT_START:
    return String(td->_client_start) + "\n";
  case H_CLIENT_INTERVAL:
    return String(td->_client_interval) + "\n";
  case H_COUNT:
    return String(td->_count) + "\n";
  case H_REQUESTS_AT_ONCE:
    return String(td->_requests_at_once) + "\n";
  case H_TIMEOUT:
    return String(td->_timeout) + "\n";
  case H_ACTIVE:
    return String(td->_active) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  ARPClient *f = (ARPClient *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: 
    {    //debug
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("debug parameter must be int");
      f->_debug = debug;
      break;
    }
  case H_CLIENT_IP:
    {    
      IPAddress client_ip;
      if (!cp_ip_address(s, &client_ip)) 
        return errh->error("client_ip parameter must be ip address");
      f->_client_ip = client_ip;
      break;
    }
  case H_CLIENT_ETHERNET:
    {    
      EtherAddress client_ethernet;
      if (!cp_ethernet_address(s, &client_ethernet)) 
        return errh->error("client_ethernet parameter must be ether address");
      f->_client_ethernet = client_ethernet;
      break;
    }
  case H_START_RANGE:
    {    
      IPAddress start_range;
      if (!cp_ip_address(s, &start_range)) 
        return errh->error("start_range parameter must be ip address");
      f->_start_range = start_range;
      break;
    }
  case H_RANGE:
    {    
      int range;
      if (!cp_integer(s, &range)) 
        return errh->error("range parameter must be int");
      f->_range = range;
      break;
    }
  case H_CLIENT_START:
    {    
      int client_start;
      if (!cp_integer(s, &client_start)) 
        return errh->error("client_start parameter must be int");
      f->_client_start = client_start;
      break;
    }
  case H_CLIENT_INTERVAL:
    {    
      int client_interval;
      if (!cp_integer(s, &client_interval)) 
        return errh->error("client_interval parameter must be int");
      f->_client_interval = client_interval;
      break;
    }
  case H_COUNT:
    {
      int count;
      if (!cp_integer(s, &count)) 
        return errh->error("count parameter must be int");
      f->_count = count;
      break;
    }
  case H_REQUESTS_AT_ONCE:
    { 
      int requests_at_once;
      if (!cp_integer(s, &requests_at_once)) 
        return errh->error("requests_at_once parameter must be int");
      f->_requests_at_once = requests_at_once;
      break;
    }
  case H_TIMEOUT:
    {  
      int timeout;
      if (!cp_integer(s, &timeout)) 
        return errh->error("timeout parameter must be int");
      f->_timeout = timeout;
      break;
    }
  case H_ACTIVE:
    {
      bool active;
      if (!cp_bool(s, &active)) 
        return errh->error("active parameter must be bool");
      f->_active = active;
      if (f->_active && !f->_request_timer.scheduled())
        f->_request_timer.schedule_after_msec(f->_client_start);
      else if (!f->_active && f->_request_timer.scheduled())
        f->_request_timer.unschedule();
      break;
    }
  }
  return 0;
}

void 
ARPClient::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
  
  add_read_handler("client_ip", read_param, (void *) H_CLIENT_IP);
  add_write_handler("client_ip", write_param, (void *) H_CLIENT_IP);
  
  add_read_handler("client_ethernet", read_param, (void *) H_CLIENT_ETHERNET);
  add_write_handler("client_ethernet", write_param, (void *) H_CLIENT_ETHERNET);
  
  add_read_handler("start_range", read_param, (void *) H_START_RANGE);
  add_write_handler("start_range", write_param, (void *) H_START_RANGE);
  
  add_read_handler("range", read_param, (void *) H_RANGE);
  add_write_handler("range", write_param, (void *) H_RANGE);

  add_read_handler("client_start", read_param, (void *) H_CLIENT_START);
  add_write_handler("client_start", write_param, (void *) H_CLIENT_START);
  
  add_read_handler("client_interval", read_param, (void *) H_CLIENT_INTERVAL);
  add_write_handler("client_interval", write_param, (void *) H_CLIENT_INTERVAL);
  
  add_read_handler("count", read_param, (void *) H_COUNT);
  add_write_handler("count", write_param, (void *) H_COUNT);
  
  add_read_handler("requests_at_once", read_param, (void *) H_REQUESTS_AT_ONCE);
  add_write_handler("requests_at_once", write_param, (void *) H_REQUESTS_AT_ONCE);
  
  add_read_handler("timeout", read_param, (void *) H_TIMEOUT);
  add_write_handler("timeout", write_param, (void *) H_TIMEOUT);

  add_read_handler("active", read_param, (void *) H_ACTIVE);
  add_write_handler("active", write_param, (void *) H_ACTIVE);
}

#include <click/vector.cc>
template class Vector<ARPClient::ARPClientRequest>;

CLICK_ENDDECLS
EXPORT_ELEMENT(ARPClient)
