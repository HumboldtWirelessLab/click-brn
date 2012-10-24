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
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "brn2_dhcpclient.hh"
#include "dhcpprotocol.hh"


CLICK_DECLS

BRN2DHCPClient::BRN2DHCPClient()
  : _timer(this)
{
  BRNElement::init();
}

BRN2DHCPClient::~BRN2DHCPClient()
{
}

void 
BRN2DHCPClient::cleanup(CleanupStage stage)
{
  if (CLEANUP_ROUTER_INITIALIZED != stage)
    return; 

  BRN_DEBUG("BRN2DHCPClient: end");
  BRN_INFO  ("BRN2DHCPClient: Configured Clients: %d von %d",
    count_configured_clients, _ip_range);
  BRN_DEBUG("BRN2DHCPClient:Skript%s %d %d", _hw_addr.unparse().c_str(),
    count_configured_clients, _ip_range);
}
int
BRN2DHCPClient::configure(Vector<String> &conf, ErrorHandler* errh)
{
  BRN_DEBUG("BRN2DHCPClient: Configure");

  _active = true;
  if (cp_va_kparse(conf, this, errh,
      "FIRSTETHERADDRESS", cpkP+cpkM, cpEthernetAddress, /*"First EtherAddress",*/ &_hw_addr,
      "FIRSTIP", cpkP+cpkM, cpIPAddress, /*"First IPAddress",*/ &_ip_addr,
      "RANGE", cpkP+cpkM, cpInteger, /*"Range",*/ &_ip_range,
      "STARTTIME", cpkP+cpkM, cpInteger, /*"starttime (s)",*/ &_start_time,
      "DIFF", cpkP+cpkM, cpInteger, /*"time between dhcp_packets (s)",*/ &_interval,
      "ACTIVE", cpkP, cpBool, /*"ACTIVE",*/ &_active,
      "DEBUG", cpkP, cpInteger,/* "Debug",*/ &_debug,
      cpEnd) < 0)
        return -1;
 
  return 0;
}

int
BRN2DHCPClient::initialize(ErrorHandler *)
{
  BRN_DEBUG("BRN2DHCPClient: Initialize");
  BRN_DEBUG("BRN2DHCPClient: Interval: %d", _interval);

  click_random_srandom();

  init_state();
  _timer.initialize(this);
  if (_active)
    _timer.schedule_after_msec(_start_time);

  return 0;
}

void 
BRN2DHCPClient::init_state() {
  uint8_t client_mac[6];

  request_queue.clear();
  range_index = 0;
  count_configured_clients = 0;

  int requested_ip = ntohl(_ip_addr.addr());
  memcpy( &client_mac[0], _hw_addr.data(), 6 );

  //client_mac[5] = 0;
  for( int i = 0; i < _ip_range; i++)
  {
     uint32_t xid = click_random();
     request_queue.push_back(DHCPClientInfo( xid, client_mac , htonl(requested_ip + i) ));
     client_mac[5] = ( client_mac[5] + 1 ) % 255;
  }
}

void
BRN2DHCPClient::run_timer(Timer* )
{
  //BRN_DEBUG("BRN2DHCPClient: Run_Timer");

  Packet *packet_out;

  _timer.reschedule_after_msec(_interval);
  assert((int)range_index < request_queue.size());

  if ( _ip_range > 0 )
  {

    switch (request_queue[range_index].status) {
      case DHCPINIT:
        {
          if ( (request_queue[range_index]._last_action != DHCPDISCOVER) ||
               ((Timestamp::now() - request_queue[range_index]._last_action_time).msecval() > 500) )
          {
            packet_out = dhcpdiscover(&request_queue[range_index]);
            if ( packet_out != NULL )  output(0).push(packet_out);
          }
          break;
        }
      case DHCPINITREBOOT:
      case DHCPSELECTING:
        {
          if ( (request_queue[range_index]._last_action != DHCPREQUEST) ||
               ((Timestamp::now() - request_queue[range_index]._last_action_time).msecval() > 500) )
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
          BRN_ERROR("BRN2DHCPClient: Unknown Status !!!");
          break;
        }
    }
    //range_index = ( range_index + 1 ) % _ip_range;
  }
}


void
BRN2DHCPClient::push( int, Packet *packet )
{
  Packet *packet_out;
	
  BRN_DEBUG("BRN2DHCPClient: Push");

  struct dhcp_packet *dhcp_p = (struct dhcp_packet *)packet->data();
  int index = search_dhcpclient_by_xid(dhcp_p->xid);

  if ( index == -1 
    || 0 != memcmp(request_queue[index].eth_add.data(), dhcp_p->chaddr, 6))
  {
    BRN_DEBUG("No request in queue");
    packet->kill();
    return;
  }

  request_queue[index].ip_add = IPAddress(dhcp_p->yiaddr);

  int option_size;
  uint8_t *server_id;

  server_id = DHCPProtocol::getOption(packet,DHO_DHCP_SERVER_IDENTIFIER, &option_size);

  if ( server_id != NULL )
   request_queue[index].server_add = IPAddress(server_id);

  int packet_type = DHCPProtocol::retrieve_dhcptype(packet);

  packet->kill();

  BRN_DEBUG("Packettype: %d", packet_type);
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
      BRN_ERROR("BRN2DHCPClient: unknown packet type!!!");
  }
 
}

Packet *
BRN2DHCPClient::dhcpdiscover(DHCPClientInfo *client_info)
{
  WritablePacket *dhcp_packet_out = DHCPProtocol::new_dhcp_packet();

  struct dhcp_packet *new_packet = (struct dhcp_packet *)dhcp_packet_out->data();

  BRN_DEBUG("DHCP-discover");
  DHCPProtocol::set_dhcp_header(dhcp_packet_out, BOOTREQUEST );

  new_packet->xid = client_info->xid;
  new_packet->flags = htons(BOOTP_BROADCAST);            // set Broadcast-Flag

  new_packet->secs = 0;

  memcpy((void*)&new_packet->ciaddr,client_info->ip_add.data(),4);
  memcpy(&new_packet->chaddr[0],client_info->eth_add.data(),6);      //set hw-addr

  DHCPProtocol::del_all_options(dhcp_packet_out);
  DHCPProtocol::add_option(dhcp_packet_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)"\1");

  client_info->status = DHCPINIT;
  client_info->_last_action = DHCPDISCOVER;
  client_info->_last_action_time = Timestamp::now();

  BRN_DEBUG("BRN2DHCPClient: Send Discover");
  BRN_DEBUG("BRN2DHCPClient: Packet ist %d Bytes gross", dhcp_packet_out->length());

  return(dhcp_packet_out);
}

Packet *
BRN2DHCPClient::dhcprequest(DHCPClientInfo *client_info)
{
  struct dhcp_packet *new_packet;

  WritablePacket *dhcp_packet_out = DHCPProtocol::new_dhcp_packet();
  new_packet = (struct dhcp_packet *)dhcp_packet_out->data();

  BRN_DEBUG("DHCP-request");

  DHCPProtocol::set_dhcp_header(dhcp_packet_out, BOOTREQUEST );

  new_packet->xid = client_info->xid;
  new_packet->secs = 0;
  new_packet->flags = htons(BOOTP_BROADCAST);  // set Broadcast-Flag

  memcpy(new_packet->chaddr,client_info->eth_add.data(),6);  //set hw-addr

  DHCPProtocol::del_all_options(dhcp_packet_out);

  DHCPProtocol::add_option(dhcp_packet_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)"\3");
  DHCPProtocol::add_option(dhcp_packet_out,DHO_DHCP_SERVER_IDENTIFIER,4,(uint8_t *)client_info->server_add.data());
  DHCPProtocol::add_option(dhcp_packet_out,DHO_DHCP_REQUESTED_ADDRESS,4,(uint8_t *)client_info->ip_add.data());
  DHCPProtocol::add_option(dhcp_packet_out,DHO_HOST_NAME,16,(uint8_t *)"DHCP_TEST_CLIENT");

  client_info->status = DHCPSELECTING;
  client_info->_last_action = DHCPREQUEST;
  client_info->_last_action_time = Timestamp::now();

  BRN_DEBUG("BRN2DHCPClient: send dhcp-request");

  return (dhcp_packet_out);
}

void
BRN2DHCPClient::dhcpbound(DHCPClientInfo *client_info)
{
  client_info->status = DHCPBOUND;
  client_info->_last_action = DHCPACK;
  client_info->_last_action_time = Timestamp::now();

  count_configured_clients++;

  range_index = ( range_index + 1 ) % _ip_range;

  BRN_INFO("BRN2DHCPClient: Client is configured: MAC: %s IP: %s",
    client_info->eth_add.unparse().c_str(), client_info->ip_add.unparse().c_str());

  BRN_DEBUG("BRN2DHCPClient:Skript%s %d %d", _hw_addr.unparse().c_str(),
    count_configured_clients,_ip_range);

}


Packet *
BRN2DHCPClient::dhcprenewing()
{
  return(NULL);
}

Packet *
BRN2DHCPClient::dhcprebinding()
{
  return(NULL);
}

Packet *
BRN2DHCPClient::dhcprelease(DHCPClientInfo *client_info)
{
  WritablePacket *dhcp_packet_out = DHCPProtocol::new_dhcp_packet();
  struct dhcp_packet *new_packet = (struct dhcp_packet *)dhcp_packet_out->data();

  BRN_DEBUG("BRN2DHCPClient: Release start !");

  DHCPProtocol::set_dhcp_header(dhcp_packet_out, BOOTREQUEST );
  new_packet->xid = client_info->xid;
  new_packet->secs = 0;
  new_packet->flags = BOOTP_BROADCAST;  // set Broadcast-Flag

  memcpy(new_packet->chaddr,_hw_addr.data(),6);  //set hw-addr

  memcpy((void*)&new_packet->yiaddr,client_info->ip_add.data(),4);

  DHCPProtocol::add_option(dhcp_packet_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)"\7");

  BRN_DEBUG("BRN2DHCPClient: Release end !");

  client_info->status = DHCPINIT;
  client_info->_last_action = DHCPRELEASE;

  return(dhcp_packet_out);
}

Packet *
BRN2DHCPClient::dhcpinform()
{
  return(NULL);
}

int
BRN2DHCPClient::search_dhcpclient_by_xid(int xid)
{
  for (int i = 0; i < request_queue.size(); i++ )
    if ( (int)request_queue[i].xid == xid ) return(i);

  return(-1);
}


String
BRN2DHCPClient::print_stats()
{
  StringAccum sa;

  sa << "<info hw=\"" << _hw_addr.unparse() << "\" ip_addr=\"" << _ip_addr.unparse() << "\">\n";

  sa << "\t<params ip_range=\"" << String(_ip_range) << "\" start_time=\"" << String(_start_time) << "\" interval=\"" << String(_interval) << "\" active=\"" << String(_active) << "\">\n";

  sa << "\">";

  sa << "\t<dhcpclient requested_ips=\"" << request_queue.size() << "\" interval=\"" << _interval << "\" >\n";
  for (int i = 0; i < request_queue.size(); i++ ) {
    sa << "\t\t<dhcp_request mac=\"" << request_queue[i].eth_add.unparse() << "\" ip=\"" << request_queue[i].ip_add.unparse();
    sa << "\" />\n";
  }
  sa << "\t</dhcpclient>\n";


  sa << "</info>\n";
  return sa.take_string();
}

enum {
  H_HW_ADDR,
  H_IP_ADDR,
  H_IP_RANGE,
  H_START_TIME,
  H_INTERVAL,
  H_SCHEDULED,
  H_ACTIVE,
  H_STATS
};

static String 
read_param(Element *e, void */*thunk*/)
{
  BRN2DHCPClient *td = (BRN2DHCPClient *)e;
  return td->print_stats();

}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BRN2DHCPClient *f = (BRN2DHCPClient *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
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
BRN2DHCPClient::add_handlers()
{
  BRNElement::add_handlers();

  // needed for QuitWatcher
  add_read_handler("scheduled", read_param, (void *) H_SCHEDULED);
  add_write_handler("hw_addr", write_param, (void *) H_HW_ADDR);
  add_write_handler("ip_addr", write_param, (void *) H_IP_ADDR);
  add_write_handler("ip_range", write_param, (void *) H_IP_RANGE);
  add_write_handler("start_time", write_param, (void *) H_START_TIME);
  add_write_handler("interval", write_param, (void *) H_INTERVAL);

  add_write_handler("active", write_param, (void *) H_ACTIVE);

  add_read_handler("stats", read_param, (void *) H_STATS);
}

#include <click/vector.cc>
template class Vector<BRN2DHCPClient::DHCPClientInfo>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DHCPClient)
