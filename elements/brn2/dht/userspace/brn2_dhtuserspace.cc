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

#include "brn2_dhtuserspace.hh"

CLICK_DECLS

DHTUserspace::DHTUserspace() :
  _debug(Brn2Logger::DEFAULT)
{
}

DHTUserspace::~DHTUserspace()
{
  BRN_DEBUG("DHTUserspace: end: free all");

  for ( int i = 0; i < client_info_list.size(); i++ )
  {
    client_info_list[i]->_client_packet->kill();
    client_info_list[i]->_client_packet = NULL;
    delete client_info_list[i];
    client_info_list.erase(client_info_list.begin() + i);
  }
}

int
DHTUserspace::configure(Vector<String> &conf, ErrorHandler* errh)
{
  BRN_DEBUG("DHTUserspace: Configure");

  int port;

  if (cp_va_kparse(conf, this, errh,
      "DHTSTORAGE", cpkN, cpElement, &_dht_storage,
      "SERVERIP", cpkP+cpkM, cpIPAddress, &_dht_ip,
      "SERVERPORT", cpkP+cpkM, cpInteger, &port,
      "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  _dht_port = (uint16_t)port;

  return 0;
}

int
DHTUserspace::initialize(ErrorHandler *)
{
  return 0;
}

static void callback_func(void *e, DHTOperation *op)
{
  DHTUserspace *s = (DHTUserspace *)e;
  DHTUserspace::UserspaceClientInfo *client_info = s->get_client_by_dht_id(op->get_id());

  if ( client_info != NULL ) {
    s->handle_dht_reply(client_info,op);
  } else {
    click_chatter("No Client info for DHT-ID");
    delete op;
  }
}

void
DHTUserspace::dht_request(UserspaceClientInfo *client_info, DHTOperation *op)
{
  uint32_t result = _dht_storage->dht_request(op, callback_func, (void*)this );

  if ( result == 0 )
  {
    click_chatter("Got direct-reply (local)");
    handle_dht_reply(client_info,op);
  } else {
    client_info->_id = result;
  }
}

void
DHTUserspace::handle_dht_reply(UserspaceClientInfo *client_info, DHTOperation *op)
{
//  int result;

  BRN_DEBUG("DHTUserspace: Handle DHT-Answer");
  if ( op->header.status == DHT_STATUS_KEY_NOT_FOUND )
  {
    WritablePacket *ans = WritablePacket::make( 128, NULL, 100   , 32);


    client_info->_client_packet = NULL;                  //packet is send (and away) so remove reference)
    output(0).push(ans);
  }

  delete op;
  remove_client(client_info);
}

void
DHTUserspace::push( int port, Packet *p_in )
{
//  int result = -1;
  char key[100];
  char *want_key;

  BRN_DEBUG("DHTUserspace: PUSH an Port %d",port);

  if ( port == 0 )
  {
    struct dht_op_header *h = (struct dht_op_header*)p_in->data();
    want_key = (char*)&(p_in->data()[sizeof(struct dht_op_header)]);
    memcpy(key,want_key,h->key_len);
    key[h->key_len] ='\0';

    click_chatter("request for %s",key);
    WritablePacket *ans = WritablePacket::make( 128, NULL, 100   , 32);

    char *buf = (char*)ans->data();

    sprintf(buf,"dhtstorage");
    output(0).push(ans);
  }
}

DHTUserspace::UserspaceClientInfo *
DHTUserspace::get_client_by_dht_id(uint32_t id)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( client_info_list[i]->_id == id ) return( client_info_list[i] );

  return( NULL );

}

int
DHTUserspace::remove_client(UserspaceClientInfo *client_info)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( client_info_list[i]->_id == client_info->_id )
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
  DHTUserspace *td = (DHTUserspace *)e;
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
  DHTUserspace *f = (DHTUserspace *)e;
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
DHTUserspace_read_param(Element */*e*/, void *thunk)
{
//  DHTUserspace *dus = (DHTUserspace *)e;

  switch ((uintptr_t) thunk)
  {
    default:              return String();
  }
  return String();
}

void
DHTUserspace::add_handlers()
{
  add_read_handler("info", DHTUserspace_read_param , (void *)H_SERVER_INFO);
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

#include <click/vector.cc>
template class Vector<DHTUserspace::UserspaceClientInfo *>;

CLICK_ENDDECLS
ELEMENT_REQUIRES(brn_common)
EXPORT_ELEMENT(DHTUserspace)
