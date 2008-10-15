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
 * dhcpclient.{cc,hh} -- responds to dhcp requests
 */

#include <click/config.h>
#include "common.hh"
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>


#include "dhcprequester.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>


#include "dhcppacketutil.hh"
CLICK_DECLS

DHCPRequester::DHCPRequester()
  : _debug(BrnLogger::DEFAULT), _timer(this)
{
}

DHCPRequester::~DHCPRequester()
{
}

void 
DHCPRequester::cleanup(CleanupStage stage)
{
  if (CLEANUP_ROUTER_INITIALIZED != stage)
    return; 

  BRN_DEBUG("DHCPRequester: end");
  BRN_INFO  ("DHCPRequester: Configured Clients: %d von %d", 
    count_configured_clients, _ip_range);
  BRN_DEBUG("DHCPRequester:Skript%s %d %d", _hw_addr.s().c_str(), 
    count_configured_clients, _ip_range);
}
int
DHCPRequester::configure(Vector<String> &conf, ErrorHandler* errh)
{
  BRN_DEBUG("DHCPRequester: Configure");

  _active = true;
  if (cp_va_parse(conf, this, errh,
      cpOptional,
        cpEthernetAddress, "First EtherAddress", &_hw_addr,
        cpIPAddress, "First IPAddress", &_ip_addr,
        cpInteger, "Range", &_ip_range,
        cpInteger, "starttime (s)", &_start_time,
        cpInteger, "time between dhcp_packets (s)", &_interval,
      cpKeywords,
        "ACTIVE", cpBool, "ACTIVE", &_active,
        "DEBUG", cpInteger, "Debug", &_debug,
      cpEnd) < 0)
        return -1;
 
  return 0;
}

int
DHCPRequester::initialize(ErrorHandler *)
{
  BRN_DEBUG("DHCPRequester: Initialize");
  BRN_DEBUG("DHCPRequester: Interval: %d", _interval);

  init_state();
  _timer.initialize(this);
  if (_active)
    _timer.schedule_after_msec(_start_time);

  return 0;
}

void 
DHCPRequester::init_state() {
  uint8_t client_mac[6];

  request_queue.clear();
  range_index = 0;
  count_configured_clients = 0;

  int requested_ip = ntohl(_ip_addr.addr());
  memcpy( &client_mac[0], _hw_addr.data(), 6 );
  
  //client_mac[5] = 0;
  for( int i = 0; i < _ip_range; i++)
  { 
     //client_mac[5] = ( client_mac[5] + i ) % 255;
     uint32_t xid = random();
     request_queue.push_back(DHCPClientInfo( xid, client_mac , htonl(requested_ip + i) ));
  }
}

void
DHCPRequester::run_timer(Timer* )
{
  //BRN_DEBUG("DHCPRequester: Run_Timer");

  Packet *packet_out;

  _timer.reschedule_after_msec(_interval);
  assert(range_index < request_queue.size());

  if ( _ip_range > 0 )
  {

    switch (request_queue[range_index].status) {
      case DHCPINIT:
        {
          if ( request_queue[range_index]._last_action != DHCPDISCOVER )
          {
            packet_out = dhcpdiscover(&request_queue[range_index]);
            if ( packet_out != NULL )  output(0).push(packet_out);
          }
          break;
        }
      case DHCPINITREBOOT:
      case DHCPSELECTING:
        {
          if ( request_queue[range_index]._last_action != DHCPREQUEST )
          {
            packet_out = dhcprequest(&request_queue[range_index]);
            if ( packet_out != NULL )  output(1).push(packet_out);
          }
          break;
        }
      case DHCPBOUND:
        {
        /*  packet_out = dhcprelease(&request_queue[range_index]);
          if ( packet_out != NULL )  output(0).push(packet_out);*/
          break;
        }
      case DHCPRENEWING:
        break;
      case DHCPREBINDING:
        break;
      default:
        {
          BRN_ERROR("DHCPRequester: Unknown Status !!!");
          break;
        }
    }
    range_index = ( range_index + 1 ) % _ip_range;
  }
}


void
DHCPRequester::push( int, Packet *packet )  
{
  Packet *packet_out;
	
  BRN_DEBUG("DHCPRequester: Push");

  struct dhcp_packet *dhcp_p = (struct dhcp_packet *)packet->data();
  int index = search_dhcpclient_by_xid(dhcp_p->xid);
  
  if ( index == -1 
    || 0 != memcmp(request_queue[index].eth_add.data(), dhcp_p->chaddr, 6))
  {
    packet->kill();
    return;
  }

  request_queue[index].ip_add = IPAddress(dhcp_p->yiaddr);

  int option_size;
  uint8_t *server_id;

  server_id = DHCPPacketUtil::getOption(packet,DHO_DHCP_SERVER_IDENTIFIER, &option_size);

  if ( server_id != NULL )
   request_queue[index].server_add = IPAddress(server_id);
   
  int packet_type = DHCPPacketUtil::retrieve_dhcptype(packet);

  packet->kill();

  switch (packet_type) {
    case DHCPDISCOVER:	// bad
      break;
    case DHCPOFFER:
      packet_out = dhcprequest(&request_queue[index]);
      if ( packet_out != NULL ) output(1).push(packet_out);			
      break;
    case DHCPREQUEST:   //bad
      break;
    case DHCPACK:
      dhcpbound(&request_queue[index]);
      break;
    case DHCPNAK:
      request_queue[index].status = DHCPINIT;
      break;
    case DHCPRELEASE:    //bad
      break;
    case DHCPDECLINE:    //bad
      break;
    case DHCPINFORM:     //bad
      break;
    default:
      BRN_ERROR("DHCPRequester: unknown packet type!!!");
  }
 
}

Packet *
DHCPRequester::dhcpdiscover(DHCPClientInfo *client_info)
{
  WritablePacket *dhcp_packet_out = DHCPPacketUtil::new_dhcp_packet();
	
  struct dhcp_packet *new_packet = (struct dhcp_packet *)dhcp_packet_out->data();

  DHCPPacketUtil::set_dhcp_header(dhcp_packet_out, BOOTREQUEST );
	
  new_packet->xid = client_info->xid;
  new_packet->flags = htons(BOOTP_BROADCAST);            // set Broadcast-Flag

  new_packet->secs = 0;

  memcpy((void*)&new_packet->ciaddr,client_info->ip_add.data(),4);
  memcpy(&new_packet->chaddr[0],client_info->eth_add.data(),6);      //set hw-addr

  DHCPPacketUtil::del_all_options(dhcp_packet_out);
  DHCPPacketUtil::add_option(dhcp_packet_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)"\1");

  client_info->status = DHCPINIT;
  client_info->_last_action = DHCPDISCOVER;

  BRN_DEBUG("DHCPRequester: Send Discover");
  BRN_DEBUG("DHCPRequester: Packet ist %d Bytes gross", dhcp_packet_out->length());
	
  return(dhcp_packet_out);
}

Packet *
DHCPRequester::dhcprequest(DHCPClientInfo *client_info)
{
  struct dhcp_packet *new_packet;

  WritablePacket *dhcp_packet_out = DHCPPacketUtil::new_dhcp_packet();
  new_packet = (struct dhcp_packet *)dhcp_packet_out->data();

  DHCPPacketUtil::set_dhcp_header(dhcp_packet_out, BOOTREQUEST );

  new_packet->xid = client_info->xid;
  new_packet->secs = 0;
  new_packet->flags = htons(BOOTP_BROADCAST);  // set Broadcast-Flag
  
  memcpy(new_packet->chaddr,client_info->eth_add.data(),6);  //set hw-addr
	
  DHCPPacketUtil::del_all_options(dhcp_packet_out);

  DHCPPacketUtil::add_option(dhcp_packet_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)"\3");
  DHCPPacketUtil::add_option(dhcp_packet_out,DHO_DHCP_SERVER_IDENTIFIER,4,(uint8_t *)client_info->server_add.data());
  DHCPPacketUtil::add_option(dhcp_packet_out,DHO_DHCP_REQUESTED_ADDRESS,4,(uint8_t *)client_info->ip_add.data());
  DHCPPacketUtil::add_option(dhcp_packet_out,DHO_HOST_NAME,16,(uint8_t *)"DHCP_TEST_CLIENT");
	
  client_info->status = DHCPSELECTING;
  client_info->_last_action = DHCPREQUEST;

  BRN_DEBUG("DHCPRequester: send dhcp-request");

  return (dhcp_packet_out);
}

void
DHCPRequester::dhcpbound(DHCPClientInfo *client_info)
{
  client_info->status = DHCPBOUND;
  client_info->_last_action = DHCPACK;

  count_configured_clients++;

  BRN_INFO("DHCPRequester: Client is configured: MAC: %s IP: %s", 
    client_info->eth_add.s().c_str(), client_info->ip_add.s().c_str());

  BRN_DEBUG("DHCPRequester:Skript%s %d %d", _hw_addr.s().c_str(), 
    count_configured_clients,_ip_range);

}


Packet *
DHCPRequester::dhcprenewing()
{
  return(NULL);
}

Packet *
DHCPRequester::dhcprebinding()
{
  return(NULL);
}

Packet *
DHCPRequester::dhcprelease(DHCPClientInfo *client_info)
{
  WritablePacket *dhcp_packet_out = DHCPPacketUtil::new_dhcp_packet();
  struct dhcp_packet *new_packet = (struct dhcp_packet *)dhcp_packet_out->data();
	
  BRN_DEBUG("DHCPRequester: Release start !");

  DHCPPacketUtil::set_dhcp_header(dhcp_packet_out, BOOTREQUEST );
  new_packet->xid = client_info->xid;
  new_packet->secs = 0;
  new_packet->flags = BOOTP_BROADCAST;  // set Broadcast-Flag

  memcpy(new_packet->chaddr,_hw_addr.data(),6);  //set hw-addr
	
  memcpy((void*)&new_packet->yiaddr,client_info->ip_add.data(),4);

  DHCPPacketUtil::add_option(dhcp_packet_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)"\7");

  BRN_DEBUG("DHCPRequester: Release end !");

  client_info->status = DHCPINIT;
  client_info->_last_action = DHCPRELEASE;

  return(dhcp_packet_out);
}

Packet *
DHCPRequester::dhcpinform()
{
  return(NULL);
}

int
DHCPRequester::search_dhcpclient_by_xid(int xid)
{
  for (int i = 0; i < request_queue.size(); i++ )
    if ( (int)request_queue[i].xid == xid ) return(i);
 
  return(-1);
}

enum {
  H_DEBUG,
  H_HW_ADDR,
  H_IP_ADDR,
  H_IP_RANGE,
  H_START_TIME,
  H_INTERVAL,
  H_SCHEDULED,
  H_ACTIVE
};

static String 
read_param(Element *e, void *thunk)
{
  DHCPRequester *td = (DHCPRequester *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_HW_ADDR:
    return td->_hw_addr.s() + "\n";
  case H_IP_ADDR:
    return td->_ip_addr.s() + "\n";
  case H_IP_RANGE:
    return String(td->_ip_range) + "\n";
  case H_START_TIME:
    return String(td->_start_time) + "\n";
  case H_INTERVAL:
    return String(td->_interval) + "\n";
  case H_SCHEDULED:
    return "false\n";
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
  DHCPRequester *f = (DHCPRequester *)e;
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
  case H_HW_ADDR:
    {    
      EtherAddress hw_addr;
      if (!cp_ethernet_address(s, &hw_addr)) 
        return errh->error("hw_addr parameter must be ether address");
      f->_hw_addr = hw_addr;
      f->init_state();
      break;
    }
  case H_IP_ADDR:
    {    
      IPAddress ip_addr;
      if (!cp_ip_address(s, &ip_addr)) 
        return errh->error("ip_addr parameter must be ip address");
      f->_ip_addr = ip_addr;
      f->init_state();
      break;
    }
  case H_IP_RANGE:
    {    
      int ip_range;
      if (!cp_integer(s, &ip_range)) 
        return errh->error("ip_range parameter must be int");
      f->_ip_range = ip_range;
      f->init_state();
      break;
    }
  case H_START_TIME:
    {    
      int start_time;
      if (!cp_integer(s, &start_time)) 
        return errh->error("start_time parameter must be int");
      f->_start_time = start_time;
      break;
    }
  case H_INTERVAL:
    {    
      int interval;
      if (!cp_integer(s, &interval)) 
        return errh->error("interval parameter must be int");
      f->_interval = interval;
      break;
    }
  case H_ACTIVE:
    {
      bool active;
      if (!cp_bool(s, &active)) 
        return errh->error("active parameter must be bool");
      f->_active = active;
      if (f->_active && !f->_timer.scheduled()) {
        f->init_state();
        f->_timer.schedule_after_msec(f->_start_time);
      }
      else if (!f->_active && f->_timer.scheduled())
        f->_timer.unschedule();
      break;
    }
  }
  return 0;
}

void
DHCPRequester::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_param, (void *) H_SCHEDULED);

  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);

  add_read_handler("hw_addr", read_param, (void *) H_HW_ADDR);
  add_write_handler("hw_addr", write_param, (void *) H_HW_ADDR);
  
  add_read_handler("ip_addr", read_param, (void *) H_IP_ADDR);
  add_write_handler("ip_addr", write_param, (void *) H_IP_ADDR);
  
  add_read_handler("ip_range", read_param, (void *) H_IP_RANGE);
  add_write_handler("ip_range", write_param, (void *) H_IP_RANGE);

  add_read_handler("start_time", read_param, (void *) H_START_TIME);
  add_write_handler("start_time", write_param, (void *) H_START_TIME);
  
  add_read_handler("interval", read_param, (void *) H_INTERVAL);
  add_write_handler("interval", write_param, (void *) H_INTERVAL);

  add_read_handler("active", read_param, (void *) H_ACTIVE);
  add_write_handler("active", write_param, (void *) H_ACTIVE);
}

#include <click/vector.cc>
template class Vector<DHCPRequester::DHCPClientInfo>;

CLICK_ENDDECLS
ELEMENT_REQUIRES(dhcp_packet_util)
EXPORT_ELEMENT(DHCPRequester)
