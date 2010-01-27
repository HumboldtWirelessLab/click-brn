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

int
DHTProtocolFalcon::pack_lp(uint8_t *buffer, int buffer_len, DHTnode *me, DHTnodelist *nodes)
{
  struct dht_falcon_node_entry *ne = (struct dht_falcon_node_entry*)buffer;
  ne->age_sec = 0;
  ne->status = me->_status;
  memcpy(ne->etheraddr, me->_ether_addr.data(), 6);

  return sizeof(dht_falcon_node_entry);
}

int
DHTProtocolFalcon::unpack_lp(uint8_t *buffer, int buffer_len, DHTnode *first, DHTnodelist *nodes)
{
  struct dht_falcon_node_entry *ne = (struct dht_falcon_node_entry*)buffer;

  first->_age = Timestamp(ne->age_sec);
  first->_status = ne->status;
  first->set_update_addr(ne->etheraddr);
  return 0;
}

WritablePacket *
DHTProtocolFalcon::new_route_request_packet(DHTnode *src, DHTnode *dst, uint8_t operation, int request_position)
{
  struct falcon_routing_packet *request;
  uint8_t *data;

  WritablePacket *rreq_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, ROUTETABLE_REQUEST, sizeof(struct falcon_routing_packet));

  data = (uint8_t*)DHTProtocol::get_payload(rreq_p);
  request = (struct falcon_routing_packet*)data;

  request->operation = operation;
  request->status = 0;
  request->table_position = request_position;

  memcpy(request->etheraddr, dst->_ether_addr.data(), 6);
  memcpy(request->src_ea, src->_ether_addr.data(), 6);

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rreq_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

WritablePacket *
DHTProtocolFalcon::new_route_reply_packet(DHTnode *src, DHTnode *dst, uint8_t operation, DHTnode *node, int request_position)
{
  struct falcon_routing_packet *reply;
  uint8_t *data;

  WritablePacket *rrep_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, ROUTETABLE_REPLY, sizeof(struct falcon_routing_packet));

  data = (uint8_t*)DHTProtocol::get_payload(rrep_p);
  reply = (struct falcon_routing_packet*)data;

  reply->operation = operation;
  reply->status = 0;
  reply->table_position = request_position;

  memcpy(reply->etheraddr, node->_ether_addr.data(), 6);
  memcpy(reply->src_ea, dst->_ether_addr.data(), 6);

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rrep_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

WritablePacket *
DHTProtocolFalcon::fwd_route_request_packet(DHTnode *src, DHTnode *new_dst, DHTnode *fwd, Packet *p)
{
  struct falcon_routing_packet *rreq;
  uint8_t *data;

  data = (uint8_t*)DHTProtocol::get_payload(p);
  rreq = (struct falcon_routing_packet*)data;

  memcpy(rreq->etheraddr, new_dst->_ether_addr.data(), 6);
  memcpy(rreq->src_ea, src->_ether_addr.data(), 6);

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(p->uniqueify(), &(fwd->_ether_addr), &(new_dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

void
DHTProtocolFalcon::get_info(Packet *p, DHTnode *src, DHTnode *node, uint8_t *operation, uint8_t *status, uint8_t *pos)
{
  struct falcon_routing_packet *request;
  uint8_t *data;

  data = (uint8_t*)DHTProtocol::get_payload(p);
  request = (struct falcon_routing_packet*)data;

  *operation = request->operation;
  *status = request->status;
  *pos = request->table_position;

  src->set_update_addr(request->src_ea);
  node->set_update_addr(request->etheraddr);
}

int
DHTProtocolFalcon::get_operation(Packet *p)
{
  struct falcon_routing_packet *request;
  uint8_t *data;

  data = (uint8_t*)DHTProtocol::get_payload(p);
  request = (struct falcon_routing_packet*)data;
  return request->operation;
}

DHTnode *
DHTProtocolFalcon::get_src(Packet *p)
{
  struct falcon_routing_packet *request;
  uint8_t *data;

  data = (uint8_t*)DHTProtocol::get_payload(p);
  request = (struct falcon_routing_packet*)data;

  DHTnode *node = new DHTnode(EtherAddress(request->src_ea));
  node->_status = STATUS_OK;
  node->_age = Timestamp::now();

  return node;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
ELEMENT_PROVIDES(DHTProtocolFalcon)
