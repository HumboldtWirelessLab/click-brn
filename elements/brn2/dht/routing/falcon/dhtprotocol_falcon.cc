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

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "dhtprotocol_falcon.hh"

CLICK_DECLS

WritablePacket *
DHTProtocolFalcon::new_hello_packet(EtherAddress *etheraddr)       //TODO: Using DHTnode
{
  struct dht_falcon_header *header;
  struct dht_falcon_node_entry *entry;
  uint8_t *data;

  WritablePacket *hello_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, HELLO,
                                                        sizeof(dht_falcon_header) + sizeof(struct dht_falcon_node_entry));

  data = (uint8_t*)DHTProtocol::get_payload(hello_p);
  header = (struct dht_falcon_header*)data;
  entry = (struct dht_falcon_node_entry*)&(data[sizeof(struct dht_falcon_header)]);

  header->status = 0;
  memcpy(entry->etheraddr, etheraddr, 6);
  entry->age_sec = 0;
  entry->status = STATUS_OK;

  return(hello_p);
}

WritablePacket *
DHTProtocolFalcon::new_hello_request_packet(EtherAddress *etheraddr)       //TODO: Using DHTnode
{
  struct dht_falcon_header *header;
  struct dht_falcon_node_entry *entry;
  uint8_t *data;

  WritablePacket *hello_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, HELLO_REQUEST,sizeof(struct dht_falcon_node_entry));

  data = (uint8_t*)DHTProtocol::get_payload(hello_p);
  header = (struct dht_falcon_header*)data;
  entry = (struct dht_falcon_node_entry*)&(data[sizeof(struct dht_falcon_header)]);

  header->status = 0;
  memcpy(entry->etheraddr, etheraddr, 6);
  entry->age_sec = 0;
  entry->status = STATUS_OK;

  return(hello_p);
}

WritablePacket *
DHTProtocolFalcon::new_route_request_packet(DHTnode *src)
{
  struct dht_falcon_header *header;
  struct dht_falcon_node_entry *entry;
  uint8_t *data;

  WritablePacket *hello_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, ROUTETABLE_REQUEST, sizeof(struct dht_falcon_node_entry));

  data = (uint8_t*)DHTProtocol::get_payload(hello_p);
  header = (struct dht_falcon_header*)data;
  entry = (struct dht_falcon_node_entry*)&(data[sizeof(struct dht_falcon_header)]);

  header->status = 0;
  memcpy(entry->etheraddr, src->_ether_addr.data(), 6);
  entry->age_sec = 0;
  entry->status = STATUS_OK;

  return(hello_p);
}

WritablePacket *
DHTProtocolFalcon::new_route_reply_packet(DHTnode *me, DHTnode *pred, DHTnode *succ, DHTnodelist *finger)
{
  struct dht_falcon_header *header;
  struct dht_falcon_node_entry *entry;
  DHTnode *node;
  uint8_t *data;

  uint8_t listsize = 0;
  uint8_t status = 0;

  if ( finger != NULL ) {
    listsize = finger->size();
    status = finger->size();
  }

  if ( pred != NULL ) {
    listsize++;
    status |= HAS_PREDECESSOR;
  }

  if ( succ != NULL ) {
    listsize++;
    status |= HAS_SUCCESSOR;
  }

  WritablePacket *route_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, ROUTETABLE_REPLY, listsize * sizeof(struct dht_falcon_node_entry));

  data = (uint8_t*)DHTProtocol::get_payload(route_p);
  header = (struct dht_falcon_header*)data;
  entry = (struct dht_falcon_node_entry*)&(data[sizeof(struct dht_falcon_header)]);

  header->status = status;

  int index = 0;
  memcpy(entry[index].etheraddr, me->_ether_addr.data(), 6);
  entry[index].age_sec = me->get_age_s();
  entry[index].status = me->_status;
  index++;

  if ( pred != NULL ) {
    memcpy(entry[index].etheraddr, pred->_ether_addr.data(), 6);
    entry[index].age_sec = pred->get_age_s();
    entry[index].status = pred->_status;
    index++;
  }

  if ( succ != NULL ) {
    memcpy(entry[index].etheraddr, succ->_ether_addr.data(), 6);
    entry[index].age_sec = succ->get_age_s();
    entry[index].status = succ->_status;
    index++;
  }

  for( int i = 0; i < finger->size() ;i++,index++ ) {
    node = finger->get_dhtnode(i);
    memcpy(entry[index].etheraddr, node->_ether_addr.data(), 6);
    entry[index].age_sec = node->get_age_s();
    entry[index].status = node->_status;
  }

  return(route_p);
}

DHTnode *
DHTProtocolFalcon::get_src(Packet *p)
{
  struct dht_falcon_header *header;
  struct dht_falcon_node_entry *entry;
  uint8_t *data;

  data = (uint8_t*)DHTProtocol::get_payload(p);
  header = (struct dht_falcon_header*)data;
  entry = (struct dht_falcon_node_entry*)&(data[sizeof(struct dht_falcon_header)]);

  DHTnode *node = new DHTnode(EtherAddress(entry[0].etheraddr));
  node->_status = entry[0].status;
  node->_age = Timestamp::now() - Timestamp(entry[0].age_sec);

  return node;
}

DHTnode *
DHTProtocolFalcon::get_predecessor(Packet *p)
{
  struct dht_falcon_header *header;
  struct dht_falcon_node_entry *entry;
  uint8_t *data;
  DHTnode *node = NULL;

  data = (uint8_t*)DHTProtocol::get_payload(p);
  header = (struct dht_falcon_header*)data;
  entry = (struct dht_falcon_node_entry*)&(data[sizeof(struct dht_falcon_header)]);

  if ( ( header->status & HAS_PREDECESSOR ) == HAS_PREDECESSOR ) {
    node = new DHTnode(EtherAddress(entry[1].etheraddr));
    node->_status = entry[1].status;
    node->_age = Timestamp::now() - Timestamp(entry[1].age_sec);
  }

  return node;
}

DHTnode *
DHTProtocolFalcon::get_successor(Packet *p)
{
  struct dht_falcon_header *header;
  struct dht_falcon_node_entry *entry;
  uint8_t *data;
  DHTnode *node = NULL;

  data = (uint8_t*)DHTProtocol::get_payload(p);
  header = (struct dht_falcon_header*)data;
  entry = (struct dht_falcon_node_entry*)&(data[sizeof(struct dht_falcon_header)]);

  int index = 1;
  if ( ( header->status & HAS_PREDECESSOR ) == HAS_PREDECESSOR )
    index++;

  if ( ( header->status & HAS_SUCCESSOR ) == HAS_SUCCESSOR ) {
    node = new DHTnode(EtherAddress(entry[index].etheraddr));
    node->_status = entry[index].status;
    node->_age = Timestamp::now() - Timestamp(entry[index].age_sec);
  }

  return node;
}

DHTnodelist *
DHTProtocolFalcon::get_fingertable(Packet *p)
{
  DHTnodelist *list = new DHTnodelist();
  struct dht_falcon_header *header;
  struct dht_falcon_node_entry *entry;
  uint8_t *data;
  DHTnode *node = NULL;

  data = (uint8_t*)DHTProtocol::get_payload(p);
  header = (struct dht_falcon_header*)data;
  entry = (struct dht_falcon_node_entry*)&(data[sizeof(struct dht_falcon_header)]);

  int index = 1;
  if ( ( header->status & HAS_PREDECESSOR ) == HAS_PREDECESSOR ) index++;

  if ( ( header->status & HAS_SUCCESSOR ) == HAS_SUCCESSOR ) index++;

  for ( int i = 0; i < ( header->status & FINGERTABLESIZE ); i++, index++ ) {
    node = new DHTnode(EtherAddress(entry[index].etheraddr));
    node->_status = entry[index].status;
    node->_age = Timestamp::now() - Timestamp(entry[index].age_sec);
    list->add_dhtnode(node);
  }

  return list;
}

DHTnodelist *
DHTProtocolFalcon::get_all_nodes(Packet *p)
{
  DHTnodelist *list = new DHTnodelist();
  struct dht_falcon_header *header;
  struct dht_falcon_node_entry *entry;
  uint8_t *data;
  DHTnode *node = NULL;

  data = (uint8_t*)DHTProtocol::get_payload(p);
  header = (struct dht_falcon_header*)data;
  entry = (struct dht_falcon_node_entry*)&(data[sizeof(struct dht_falcon_header)]);

  int index = 0;
  if ( ( header->status & HAS_PREDECESSOR ) == HAS_PREDECESSOR ) {
    node = new DHTnode(EtherAddress(entry[index].etheraddr));
    node->_status = entry[index].status;
    node->_age = Timestamp::now() - Timestamp(entry[index].age_sec);
    list->add_dhtnode(node);
    index++;
  }

  /* Source after Predecessor so list is sorted */
  node = new DHTnode(EtherAddress(entry[0].etheraddr));
  node->_status = entry[0].status;
  node->_age = Timestamp::now() - Timestamp(entry[0].age_sec);
  list->add_dhtnode(node);

  if ( ( header->status & HAS_SUCCESSOR ) == HAS_SUCCESSOR ) {
    node = new DHTnode(EtherAddress(entry[index].etheraddr));
    node->_status = entry[index].status;
    node->_age = Timestamp::now() - Timestamp(entry[index].age_sec);
    list->add_dhtnode(node);
    index++;
  }

  for ( int i = 0; i < ( header->status & FINGERTABLESIZE ); i++, index++ ) {
    node = new DHTnode(EtherAddress(entry[index].etheraddr));
    node->_status = entry[index].status;
    node->_age = Timestamp::now() - Timestamp(entry[index].age_sec);
    list->add_dhtnode(node);
  }

  return list;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
ELEMENT_PROVIDES(DHTProtocolFalcon)
