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
DHTProtocolOmni::new_hello_request_packet(EtherAddress *etheraddr)       //TODO: Using DHTnode
{
  struct dht_omni_node_entry *msg; 
  WritablePacket *hello_p = DHTProtocol::new_dht_packet(ROUTING_OMNI, HELLO_REQUEST,sizeof(struct dht_omni_node_entry));

  msg = (struct dht_omni_node_entry*)DHTProtocol::get_payload(hello_p);
  memcpy(msg->etheraddr,etheraddr->data(),6);
  msg->age_sec = 0;                                                     //TODO: using NodeInfo
  msg->status = STATUS_OK;

  return(hello_p);	
}

WritablePacket *
DHTProtocolOmni::new_route_request_packet(EtherAddress *me, DHTnodelist *list)   //TODO: using DHTnode
{
  struct dht_omni_node_entry *msg;
  uint8_t listsize = 0;

  if ( list != NULL ) listsize = list->size();
  WritablePacket *route_p = DHTProtocol::new_dht_packet(ROUTING_OMNI, ROUTETABLE_REQUEST,( 1 + listsize ) * sizeof(struct dht_omni_node_entry));

  msg = (struct dht_omni_node_entry*)DHTProtocol::get_payload(route_p);
  memcpy(msg->etheraddr,me->data(),6);
  msg->age_sec = 0;                                                     //TODO: using NodeInfo
  msg->status = STATUS_OK;

  DHTnode *n;
  for( int i = 0;i < listsize ;i++ ) {
    n = list->get_dhtnode(i);
    memcpy(msg[i+1].etheraddr,n->_ether_addr.data(),6);
    msg->age_sec = 0;                                                     //TODO: using NodeInfo
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
  Timestamp now = Timestamp::now();

  for( int i = 0;i < listsize ;i++ ) {
    n = list->get_dhtnode(i);
    memcpy(msg[i+1].etheraddr,n->_ether_addr.data(),6);
    msg->age_sec = 0;                                                     //TODO: using NodeInfo
    msg[i+1].status = n->_status;
  }

  return(route_p);
}

int
DHTProtocolOmni::get_dhtnodes(Packet *p,DHTnodelist *dhtlist)
{
  uint8_t *payload;
  uint16_t payload_len;
  struct dht_omni_node_entry *entry;
  int count = 0;
  DHTnode *node;

  payload_len = DHTProtocol::get_payload_len(p);
  payload = DHTProtocol::get_payload(p);

  for ( int i = 0; i < payload_len; i += sizeof(struct dht_omni_node_entry), count++ )
  {
    entry = (struct dht_omni_node_entry*)&payload[i];
    node = new DHTnode(EtherAddress(entry->etheraddr));
    node->_status = entry->status;
    node->_age = Timestamp::now() - Timestamp(entry->age_sec);
    dhtlist->add_dhtnode(node);
  }

  return count;
}

WritablePacket *
DHTProtocolOmni::push_brn_ether_header(WritablePacket *p,EtherAddress *src, EtherAddress *dst)
{
  WritablePacket *big_p = NULL;
  struct click_brn brn_header;
  click_ether *ether_header = NULL;
  int payload_len = p->length();

  big_p = p->push(sizeof(struct click_brn) + sizeof(click_ether));

  if ( big_p == NULL ) {
    click_chatter("Push failed. No memory left ??");
  }
  else
  {
    ether_header = (click_ether *)big_p->data();
    memcpy( ether_header->ether_dhost,dst->data(),6);
    memcpy( ether_header->ether_shost,src->data(),6);
    ether_header->ether_type = htons(ETHERTYPE_BRN);
    big_p->set_ether_header(ether_header);

    brn_header.dst_port = 10;
    brn_header.src_port = 10;
    brn_header.body_length = payload_len;
    brn_header.ttl = 100;
    brn_header.tos = 0;

    memcpy((void*)&(big_p->data()[14]),(void*)&brn_header, sizeof(brn_header));
  }

  return big_p;
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
ELEMENT_PROVIDES(DHTProtocolOmni)
