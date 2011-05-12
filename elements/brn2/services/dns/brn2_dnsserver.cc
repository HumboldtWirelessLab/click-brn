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
 * dnsserver.{cc,hh} -- responds to dns requests
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
#include <click/string.hh>

#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/storage/dhtstorage.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "brn2_dnsserver.hh"
#include "dnsprotocol.hh"

CLICK_DECLS

BRN2DNSServer::BRN2DNSServer() :
  _dhcpsubnetlist(NULL),
  _vlantable(NULL),
  _server_redirect(false)
{
  BRNElement::init();
}

BRN2DNSServer::~BRN2DNSServer()
{
  if (_debug >= BrnLogger::INFO) {
    click_chatter("BRN2DNSServer: end: free all");
  }

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
      "SERVERREDIRECT", cpkP, cpBool, &_server_redirect,
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
    click_chatter("DNS-Server: No Client info for DHT-ID");
    delete op;
  }
}

void
BRN2DNSServer::dht_request(DNSClientInfo *client_info, DHTOperation *op)
{
  uint32_t result = _dht_storage->dht_request(op, callback_func, (void*)this );

  if ( result == 0 )
  {
    BRN_INFO("Got direct-reply (local)");
    handle_dht_reply(client_info,op);
  } else {
    client_info->_id = result;
  }
}

void
BRN2DNSServer::handle_dht_reply(DNSClientInfo *client_info, DHTOperation *op)
{
  //int result;

  BRN_DEBUG("BRN2DNSServer: Handle DHT-Answer");
  if ( op->header.status == DHT_STATUS_KEY_NOT_FOUND )
  {
    char *name = new char[op->header.keylen + 1];
    memcpy(name,op->key, op->header.keylen);
    name[op->header.keylen] = '\0';

    BRN_INFO("No client with name %s !",name);
    delete[] name;

  } else {
    uint16_t nameoffset = 0x0cc0;
    WritablePacket *ans = DNSProtocol::dns_question_to_answer(client_info->_client_packet, &nameoffset,
                                                               sizeof(nameoffset), 1, 1, 300,
                                                               op->header.valuelen, op->value);
    client_info->_client_packet = NULL;                  //packet is send (and away) so remove reference)
    output(0).push(ans);
  }

  delete op;
  remove_client(client_info);
}

//TODO: check dhcpsubnetlist for other domains were this dns-server is responsible for
void
BRN2DNSServer::push( int port, Packet *p_in )
{
//  int result = -1;

  BRN_DEBUG("BRN2DNSServer: PUSH an Port %d",port);

  if ( port == 0 )
  {
    BRN_DEBUG("BRN2DNSServer: Responder::dnsresponse");
    char *cname = DNSProtocol::get_name(p_in);
    String name = String(cname);
    delete[] cname;

    if ( name == _sname || name == _full_sname ) {
      BRN_INFO("Ask for me");

      uint16_t nameoffset = 0x0cc0;
      WritablePacket *ans = DNSProtocol::dns_question_to_answer(p_in, &nameoffset, sizeof(nameoffset),
                                                                1, 1, 300, 4, _server_ident.data());
      output(0).push(ans);

    } else if ( (DNSProtocol::isInDomain( name, _domain_name )) && (_dht_storage != NULL) ) {
      BRN_INFO("Ask for other in domain");

      DNSClientInfo *ci = new DNSClientInfo(p_in,IPAddress(0),name);
      client_info_list.push_back(ci);
      DHTOperation *op = new DHTOperation();
      op->read((uint8_t*)name.data(), name.length());
      dht_request(ci,op);

    } else {
      if ( _server_redirect ) {
        uint16_t nameoffset = 0x0cc0;
        WritablePacket *ans = DNSProtocol::dns_question_to_answer(p_in, &nameoffset, sizeof(nameoffset),
                                                                     1, 1, 300, 4, _server_ident.data());
        output(0).push(ans);
      } else {
        BRN_INFO("Ask for other ( %s <-> %s )",name.c_str(), _domain_name.c_str());
        output(1).push(p_in); //TODO: fragt nach anderen -> weiterleiten (NAT ??)
      }
    }
  }
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
    if ( client_info_list[i]->_id == client_info->_id )
    {
      delete client_info_list[i];
      client_info_list.erase(client_info_list.begin() + i);
    }

  return(0);
}

enum { H_SERVER_INFO};

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
  BRNElement::add_handlers();

  add_read_handler("server_info", BRN2DNSServer_read_param , (void *)H_SERVER_INFO);
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
EXPORT_ELEMENT(BRN2DNSServer)
