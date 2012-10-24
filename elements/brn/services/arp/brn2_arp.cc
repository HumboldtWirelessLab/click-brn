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
 * arp.{cc,hh}
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "brn2_arp.hh"

#include "elements/brn/dht/storage/dhtoperation.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"
#include "elements/brn/dht/storage/rangequery/ip_rangequery.hh"

CLICK_DECLS

BRN2Arp::BRN2Arp() :
  _debug(BrnLogger::DEFAULT)
{

}

BRN2Arp::~BRN2Arp()
{
}

void 
BRN2Arp::cleanup(CleanupStage stage)
{
  if (CLEANUP_ROUTER_INITIALIZED != stage)
    return; 

  BRN_DEBUG("ARP: Clean up on exit ! TODO");

  BRN_INFO("ARP: ARPReply: %d",count_arp_reply);
  BRN_INFO("ARP:Skript %d",count_arp_reply);
}

int
BRN2Arp::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String router_ip;
  if (cp_va_kparse(conf, this, errh,
    "ROUTERIP", cpkP+cpkM, cpIPAddress, &_router_ip,             /* e.g. "10.9.0.1" */
    "ROUTERETHERADDRESS", cpkP+cpkM, cpEthernetAddress, &_router_ethernet,
    "PREFIX", cpkP+cpkM, cpIPPrefix, &_src_ip, &_src_ip_mask,
    "DHTSTORAGE", cpkP+cpkM, cpElement, &_dht_storage,
    "DHCPSUBNETLIST", cpkP, cpElement, &_dhcpsubnetlist,
    "VLANTABLE", cpkP, cpElement, &_vlantable,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
BRN2Arp::initialize(ErrorHandler *)
{
  count_arp_reply = 0;
  return 0;
}

static void callback_func(void *e, DHTOperation *op)
{
  BRN2Arp *s = (BRN2Arp *)e;

  s->handle_dht_reply(op);
}

void
BRN2Arp::dht_request(DHTOperation *op)
{
  //click_chatter("send storage request");
  uint32_t result = _dht_storage->dht_request(op, callback_func, (void*)this );

  if ( result == 0 )
  {
    //click_chatter("Got direct-reply (local)");
    handle_dht_reply(op);
  }
}


void
BRN2Arp::push( int port, Packet *packet )
{
  BRN_DEBUG("ARP: PUSH\n");

  if ( port == 0 )
    handle_arp_request(packet);
}


/* Method processes an incoming packet. */
int
BRN2Arp::handle_arp_request(Packet *p)
{
  click_ether *ethp = (click_ether *) p->data();                  //etherhaeder rausholen
  click_ether_arp *arpp = (click_ether_arp *) (ethp + 1);         //arpheader rausholen

  if ( ntohs(arpp->ea_hdr.ar_op) == ARPOP_REQUEST )
  {
    BRN_DEBUG("ARP: Packet ist ARP-Request for %s\n", IPAddress(arpp->arp_tpa).unparse().c_str());

    BRN_DEBUG("ARP: %s %s\n", _src_ip.unparse().c_str(), _src_ip_mask.unparse().c_str()); 

    if ( memcmp ( _router_ip.data(),arpp->arp_tpa, 4 ) == 0                                   //ask for router_ip
        || (_src_ip_mask && !IPAddress(arpp->arp_tpa).matches_prefix(_src_ip, _src_ip_mask)) )  //TODO: use dhcpsubnetlist
    {
      if ( memcmp ( _router_ip.data(),arpp->arp_tpa, 4 ) != 0 )
        BRN_WARN("ARP-Request: Src is other subnet, so etheraddress is router-ea. Please use SUBNETLIST");
      send_arp_reply( _router_ethernet.data(), arpp->arp_tpa, arpp->arp_sha, arpp->arp_spa );
    }
    else
    {
      long time_now;
      struct timeval now;
      now = Timestamp::now().timeval();

      time_now = now.tv_sec;

      int i;

      bool in_queue = false;
      bool found_similar = false;

      for ( i = 0; i < request_queue.size(); i++)
      {
        if ( memcmp( request_queue[i]._d_ip_add, arpp->arp_tpa, 4 ) == 0 )
        {
          found_similar = true;
          time_now = request_queue[i]._time;
          if ( memcmp( request_queue[i]._s_ip_add, arpp->arp_spa, 4 ) == 0 )
          {
            in_queue = true;
            break;
          }
        }
      }

      if ( ! in_queue )
        request_queue.push_back( ARPrequest(arpp->arp_sha, arpp->arp_spa, arpp->arp_tpa, (uint8_t)request_queue.size(), time_now) );

      if ( ! found_similar )                                              //nach der IP wurde noch nicht gefragt
      {
        DHTOperation *op = new DHTOperation();
        op->read((uint8_t*)arpp->arp_tpa, 4);

        if ( _dht_storage->range_query_support() ) {
          uint8_t key_id[16];
          IPRangeQuery iprq = IPRangeQuery(_router_ip & _src_ip_mask, _src_ip_mask);
          iprq.get_id_for_value(IPAddress(arpp->arp_tpa), key_id);
          op->set_key_digest(key_id);
        }

        dht_request(op);
      }
    }
  }

  p->kill();

  return 0;
}


int
BRN2Arp::handle_dht_reply(DHTOperation *op)
{
  BRN_DEBUG("ARP: Antwort von DHT!");
/* Test: ip found ?*/	

  if ( op->header.status == DHT_STATUS_KEY_NOT_FOUND )
  {
    BRN_DEBUG("ARP: IP not found (DHT) !!!");

    for ( int i = 0; i < request_queue.size(); i++)
    {
      if ( memcmp( request_queue[i]._d_ip_add, op->key , 4 ) == 0 )
      {
#ifdef TEST_FOR_NS2
        BRN_DEBUG("ARP: IP not found but DHT works fine !!!");
        send_arp_reply( _router_ethernet.data() , request_queue[i]._d_ip_add, request_queue[i]._s_hw_add, request_queue[i]._s_ip_add );
#endif
        request_queue.erase( request_queue.begin() + i );
      }
    }
    delete op;
    BRN_DEBUG("ARP: DHTanswer end !!!");

    return(1);
  }
  else
  {
    EtherAddress _mac(op->value);
    BRN_DEBUG("ARP: MAC: %s",_mac.unparse().c_str());
    BRN_DEBUG("ARP: queuesize = %d",request_queue.size());

    for ( int i = 0; i < request_queue.size(); i++)
    {
      if ( memcmp( request_queue[i]._d_ip_add, op->key, 4 ) == 0 )
      {
        send_arp_reply(op->value, request_queue[i]._d_ip_add, request_queue[i]._s_hw_add, request_queue[i]._s_ip_add );
        request_queue.erase( request_queue.begin() + i );
      }
    }
    delete op;
    return(0);
  }
  delete op;
  return(1);
}


int
BRN2Arp::send_arp_reply( uint8_t *s_hw_add, uint8_t *s_ip_add, uint8_t *d_hw_add, uint8_t *d_ip_add )
{
  count_arp_reply++;

  click_ether *e;
  click_ether_arp *ea;

  //neues Paket
  WritablePacket *p = Packet::make(128, NULL,sizeof(click_ether)+sizeof(click_ether_arp), 32);

  p->set_mac_header(p->data(), 14);
  memset(p->data(), '\0', p->length());

  //pointer auf Ether-header holen
  e = (click_ether *) p->data();
  //Pointer auf BRN2Arp-Header holen
  ea = (click_ether_arp *) (e + 1);

  //alte Quelle ist neues Ziel des Etherframes
  memcpy(e->ether_dhost, d_hw_add, 6);
  memcpy(e->ether_shost, s_hw_add, 6);

  //type im Ether-Header auf ARP setzten
  e->ether_type = htons(ETHERTYPE_ARP);

  //Felder von ARP füllen
  //Type der Hardware Adresse setzen
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
  //Type der Protokoll Adresse setzten
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
  //Laenge setzten
  ea->ea_hdr.ar_hln = 6;
  //Laenge
  ea->ea_hdr.ar_pln = 4;
  //arp_opcode auf Replay setzen
  ea->ea_hdr.ar_op = htons(ARPOP_REPLY);

  //neues Packet basteln, erstmal quell und target zeug vertauschen
  memcpy(ea->arp_tha, d_hw_add, 6);                         //neues Target ist altes Source
  memcpy(ea->arp_tpa, d_ip_add, 4);                         //neues Target ist altes Source
  memcpy(ea->arp_sha, s_hw_add, 6);                         //neues Source der Tabelle entnehmen
  memcpy(ea->arp_spa, s_ip_add, 4);                         //neues Source der Tabelle entnehmen

  BRN_DEBUG("ARP: send ARP-Reply");

  output( 0 ).push( p );

  return(0);

}

int 
BRN2Arp::send_arp_request( uint8_t *s_hw_add, uint8_t *s_ip_add, uint8_t *d_hw_add, uint8_t *d_ip_add )
{
  WritablePacket *q = Packet::make(sizeof(click_ether) + sizeof(click_ether_arp));
  BRN_CHECK_EXPR_RETURN(!q,
    ("cannot make packet"), return (-1));
  memset(q->data(), '\0', q->length());

  click_ether *e = (click_ether *) q->data();
  q->set_ether_header(e);
  memcpy(e->ether_dhost, "\xff\xff\xff\xff\xff\xff", 6);
  memcpy(e->ether_shost, s_hw_add, 6);
  e->ether_type = htons(ETHERTYPE_ARP);

  click_ether_arp *ea = (click_ether_arp *) (e + 1);
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
  ea->ea_hdr.ar_hln = 6;
  ea->ea_hdr.ar_pln = 4;
  ea->ea_hdr.ar_op = htons(ARPOP_REQUEST);
  memcpy(ea->arp_tpa, d_ip_add, 4);
  memcpy(ea->arp_tha, d_hw_add, 6);
  memcpy(ea->arp_spa, s_ip_add, 4);
  memcpy(ea->arp_sha, s_hw_add, 6);

  output( 0 ).push( q );
  return (0);
}

void 
BRN2Arp::send_gratious_arp()
{
  BRN_DEBUG("sending gratious arp");

  send_arp_request( _router_ethernet.data(), 
                    _router_ip.data(), 
                    _router_ethernet.data(), 
                    _router_ip.data() );

}

enum {H_DEBUG, H_SENDARP};

static String 
read_param(Element *e, void *thunk)
{
  BRN2Arp *td = (BRN2Arp *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_SENDARP:
    return String("N/A\n");
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BRN2Arp *f = (BRN2Arp *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_SENDARP:
    {    //debug
      EtherAddress sta;
      f->send_gratious_arp();
      break;
    }
  }
  return 0;
}

void 
BRN2Arp::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
  
  add_write_handler("send_gratious_arp", write_param, (void *) H_SENDARP);
}

#include <click/vector.cc>
template class Vector<BRN2Arp::ARPrequest>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Arp)
