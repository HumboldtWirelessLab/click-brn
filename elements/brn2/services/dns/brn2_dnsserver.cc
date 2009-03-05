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
 * dhcpserver.{cc,hh} -- responds to dhcp requests
 */

#include <click/config.h>
#include "elements/brn/common.hh"

#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>

#include "brn2_dnsserver.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/string.hh>

#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/storage/dhtstorage.hh"

#include "dnsprotocol.hh"

CLICK_DECLS

BRN2DNSServer::BRN2DNSServer() :
  _debug(BrnLogger::DEFAULT),
  _dhcpsubnetlist(NULL),
  _vlantable(NULL)
{
}

BRN2DNSServer::~BRN2DNSServer()
{
  BRN_DEBUG("BRN2DNSServer: end: free all");

  for ( int i = 0; i < client_info_list.size(); i++ )
  {
    client_info_list[i]->_client_packet->kill();
    delete client_info_list[i];
    client_info_list.erase(client_info_list.begin() + i);
  }
}

int
BRN2DNSServer::configure(Vector<String> &conf, ErrorHandler* errh)
{
    BRN_DEBUG("BRN2DNSServer: Configure");

  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP, cpEthernetAddress, &_me,
      "SERVER", cpkP, cpIPAddress, &_server_ident,
      "SERVERNAME", cpkP, cpString, &_sname,
      "DOMAIN", cpkP, cpString, &_domain_name,
      "DHTSTORAGE", cpkP, cpElement, &_dht_storage,
      "DHCPSUBNETLIST", cpkP, cpElement, &_dhcpsubnetlist,
      "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
    cpEnd) < 0)
      return -1;

  _full_sname = String(_sname.c_str());
  _full_sname += _domain_name;

  return 0;

}

int
BRN2DNSServer::initialize(ErrorHandler *)
{
  return 0;
}

static void callback_func(void *e, DHTOperation *op)
{
  BRN2DNSServer *s = (BRN2DNSServer *)e;

  BRN2DNSServer::DNSClientInfo *client_info = s->get_client_by_dht_id(op->get_id());

  if ( client_info != NULL ) {
    s->handle_dht_reply(client_info,op);
  } else {
    click_chatter("No Client info for DHT-ID");
    delete op;
  }
}

void
BRN2DNSServer::dht_request(DNSClientInfo *client_info, DHTOperation *op)
{
  //click_chatter("send storage request");
  uint32_t result = _dht_storage->dht_request(op, callback_func, (void*)this );

  if ( result == 0 )
  {
    //click_chatter("Got direct-reply (local)");
    handle_dht_reply(client_info,op);
  } else {
    client_info->_id = result;
  }
}

void
BRN2DNSServer::handle_dht_reply(DNSClientInfo *client_info, DHTOperation *op)
{
  int result;

  BRN_DEBUG("BRN2DNSServer: Handle DHT-Answer");

}

//Frage von client -> frage an dht -> antwort von dht -> antwort für client

void
BRN2DNSServer::push( int port, Packet *p_in )
{
  int result = -1;

  BRN_DEBUG("BRN2DNSServer: PUSH an Port %d",port);

  if ( port == 0 )
  {
    BRN_DEBUG("BRN2DNSServer: Responder::dhcpresponse");
    String name = String(DNSProtocol::get_name(p_in));
    click_chatter("Fullname: %s Sname: %s",_full_sname.c_str(),_sname.c_str());
    click_chatter("Frage nach : %s",name.c_str());

    if ( name == _sname || name == _full_sname ) {
      click_chatter("fragt nach mir");

      uint16_t nameoffset = 0x0cc0;
      WritablePacket *ans = DNSProtocol::dns_question_to_answer(p_in, &nameoffset, sizeof(nameoffset),
                                                                1, 1, 300, 4, _server_ident.data());
      output(0).push(ans);

    } else if ( DNSProtocol::isInDomain( name, _domain_name ) ) {
      click_chatter("fragt nach rechner der domain");

    } else {
       click_chatter("fragt nach anderen");
      //TODO: fragt nach anderen -> weiterleiten (NAT ??)
      //output(1).push(p_in);
    }
  }
}

int
BRN2DNSServer::send_dht_request(DNSClientInfo *client_info)
{
  BRN_DEBUG("BRN2DNSServer: Packet for DHT");

  DHTOperation *op;

  op = new DHTOperation();
  op->read((uint8_t*) &client_info->_ciaddr, 4);
  dht_request(client_info,op);

  return(0);
}


BRN2DNSServer::DNSClientInfo *
BRN2DNSServer::get_client_by_dht_id(uint32_t id)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( client_info_list[i]->_id == id ) return( client_info_list[i] );

  return( NULL );

}

int
BRN2DNSServer::remove_client(DNSClientInfo *client_info)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( memcmp(client_info_list[i]->_chaddr, client_info->_chaddr, 6 ) == 0 )
    {
      delete client_info_list[i];
      client_info_list.erase(client_info_list.begin() + i);
    }

  return(0);
}

enum { H_SERVER_INFO, H_DEBUG };

static String
read_param(Element *e, void *thunk)
{
  BRN2DNSServer *td = (BRN2DNSServer *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}

static int
write_param(const String &in_s, Element *e, void *vparam,
            ErrorHandler *errh)
{
  BRN2DNSServer *f = (BRN2DNSServer *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {    //debug
      int debug;
      if (!cp_integer(s, &debug))
        return errh->error("debug parameter must be boolean");
      f->_debug = debug;
      break;
    }
  }
  return 0;
}

static String
BRN2DNSServer_read_param(Element *e, void *thunk)
{
  BRN2DNSServer *dns = (BRN2DNSServer *)e;

  switch ((uintptr_t) thunk)
  {
    case H_SERVER_INFO :  return ( dns->server_info( ) );
    default:              return String();
  }
  return String();
}

void
BRN2DNSServer::add_handlers()
{
  add_read_handler("server_info", BRN2DNSServer_read_param , (void *)H_SERVER_INFO);

  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

String
BRN2DNSServer::server_info(void)
{
 StringAccum sa;

 sa << "DNS-Server-INFO\n";
 sa << "Server: " << _server_ident << "\n";         //IP DHCP-Server
 sa << "DNS: " << _name_server.unparse() << "\n";   //IP
 sa << "Domain: " << _domain_name << "\n";

 return sa.take_string();
}


#include <click/vector.cc>
template class Vector<BRN2DNSServer::DNSClientInfo *>;

CLICK_ENDDECLS
ELEMENT_REQUIRES(brn_common dhcp_packet_util dhcp_packet_util)
EXPORT_ELEMENT(BRN2DNSServer)
