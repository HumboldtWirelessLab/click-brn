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


#include <click/config.h>
#include <click/etheraddress.hh>
#include "elements/brn/common.hh"

#include "elements/brn/dht/standard/brn2_dhtnode.hh"
#include "elements/brn/dht/standard/brn2_dhtnodelist.hh"
#include "elements/brn/dht/protocol/dhtprotocol.hh"
#include "dhtprotocol_omniscient.hh"

CLICK_DECLS


WritablePacket *
DHTProtocolOmni::new_hello_packet(EtherAddress *etheraddr)
{
  struct dht_omni_node_entry *msg;
  WritablePacket *hello_p = DHTProtocol::new_dht_packet(ROUTING_OMNI, HELLO,sizeof(struct dht_omni_node_entry));

  msg = (struct dht_omni_node_entry*)DHTProtocol::get_payload(hello_p);
  memcpy(msg->etheraddr,etheraddr->data(),6);
  msg->status = STATUS_OK;

  return(hello_p);
}

WritablePacket *
DHTProtocolOmni::new_hello_request_packet(EtherAddress *etheraddr)
{
  struct dht_omni_node_entry *msg; 
  WritablePacket *hello_p = DHTProtocol::new_dht_packet(ROUTING_OMNI, HELLO_REQUEST,sizeof(struct dht_omni_node_entry));

  msg = (struct dht_omni_node_entry*)DHTProtocol::get_payload(hello_p);
  memcpy(msg->etheraddr,etheraddr->data(),6);
  msg->status = STATUS_OK;

  return(hello_p);	
}


WritablePacket *
DHTProtocolOmni::new_route_request_packet(EtherAddress *me, DHTnodelist *list)
{
  struct dht_omni_node_entry *msg;
  uint8_t listsize = 0;

  if ( list != NULL ) listsize = list->size();
  WritablePacket *route_p = DHTProtocol::new_dht_packet(ROUTING_OMNI, ROUTETABLE_REQUEST,( 1 + listsize ) * sizeof(struct dht_omni_node_entry));

  msg = (struct dht_omni_node_entry*)DHTProtocol::get_payload(route_p);
  memcpy(msg->etheraddr,me->data(),6);
  msg->status = STATUS_OK;

  DHTnode *n;
  for( int i = 0;i < listsize ;i++ ) {
    n = list->get_dhtnode(i);
    memcpy(msg[i+1].etheraddr,n->_ether_addr.data(),6);
    msg[i+1].status = n->_status;
  }

  return(route_p);
}

WritablePacket *
DHTProtocolOmni::new_route_reply_packet(EtherAddress *me, DHTnodelist *list)
{
  struct dht_omni_node_entry *msg;
  uint8_t listsize = 0;

  if ( list != NULL ) listsize = list->size();
  WritablePacket *route_p = DHTProtocol::new_dht_packet(ROUTING_OMNI, ROUTETABLE_REPLY,( 1 + listsize ) * sizeof(struct dht_omni_node_entry));

  msg = (struct dht_omni_node_entry*)DHTProtocol::get_payload(route_p);
  memcpy(msg->etheraddr,me->data(),6);
  msg->status = STATUS_OK;

  DHTnode *n;
  for( int i = 0;i < listsize ;i++ ) {
    n = list->get_dhtnode(i);
    memcpy(msg[i+1].etheraddr,n->_ether_addr.data(),6);
    msg[i+1].status = n->_status;
  }

  return(route_p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
ELEMENT_PROVIDES(DHTProtocolOmni)
