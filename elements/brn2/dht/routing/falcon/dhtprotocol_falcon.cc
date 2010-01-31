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

#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "dhtprotocol_falcon.hh"

CLICK_DECLS

int
DHTProtocolFalcon::pack_lp(uint8_t *buffer, int /*buffer_len*/, DHTnode *me, DHTnodelist */*nodes*/)
{
  struct dht_falcon_node_entry *ne = (struct dht_falcon_node_entry*)buffer;
  ne->age_sec = 0;
  ne->status = me->_status;
  memcpy(ne->etheraddr, me->_ether_addr.data(), 6);

  return sizeof(dht_falcon_node_entry);
}

int
DHTProtocolFalcon::unpack_lp(uint8_t *buffer, int /*buffer_len*/, DHTnode *first, DHTnodelist */*nodes*/)
{
  struct dht_falcon_node_entry *ne = (struct dht_falcon_node_entry*)buffer;

  first->_age = Timestamp(ne->age_sec);
  first->_status = ne->status;
  first->set_update_addr(ne->etheraddr);
  return 0;
}

WritablePacket *
DHTProtocolFalcon::new_route_request_packet(DHTnode *src, DHTnode *dst, uint8_t type, int request_position)
{
  WritablePacket *rreq_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, type, sizeof(struct falcon_routing_packet));
  struct falcon_routing_packet *request = (struct falcon_routing_packet*)DHTProtocol::get_payload(rreq_p);

  request->status = 0;
  request->table_position = htons(request_position);

  DHTProtocol::set_src(rreq_p, src->_ether_addr.data());
  DHTProtocol::set_dst(rreq_p, dst->_ether_addr.data());

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rreq_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

WritablePacket *
DHTProtocolFalcon::new_route_reply_packet(DHTnode *src, DHTnode *dst, uint8_t type, DHTnode *node, int request_position)
{
  WritablePacket *rrep_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, type, sizeof(struct falcon_routing_packet));
  struct falcon_routing_packet *reply = (struct falcon_routing_packet*)DHTProtocol::get_payload(rrep_p);

  reply->status = 0;
  reply->table_position = htons(request_position);

  DHTProtocol::set_src(rrep_p, src->_ether_addr.data());
  DHTProtocol::set_dst(rrep_p, node->_ether_addr.data());

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rrep_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

WritablePacket *
DHTProtocolFalcon::fwd_route_request_packet(DHTnode *src, DHTnode *new_dst, DHTnode *fwd, Packet *p)
{
  WritablePacket *rfwd_p = p->uniqueify();

  DHTProtocol::set_src(rfwd_p, src->_ether_addr.data());
  DHTProtocol::set_dst(rfwd_p, new_dst->_ether_addr.data());

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rfwd_p, &(fwd->_ether_addr), &(new_dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

void
DHTProtocolFalcon::get_info(Packet *p, DHTnode *src, DHTnode *node, uint8_t *status, uint16_t *pos)
{
  struct falcon_routing_packet *request = (struct falcon_routing_packet*)DHTProtocol::get_payload(p);

  *status = request->status;
  *pos = ntohs(request->table_position);

  src->set_update_addr(DHTProtocol::get_src_data(p));
  node->set_update_addr(DHTProtocol::get_dst_data(p));
}

WritablePacket *
DHTProtocolFalcon::new_nws_packet(DHTnode *src, DHTnode *dst, uint32_t size)
{
  WritablePacket *rreq_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, FALCON_MINOR_NWS_REQUEST, sizeof(struct falcon_nws_packet));
  struct falcon_nws_packet *request = (struct falcon_nws_packet*)DHTProtocol::get_payload(rreq_p);

  request->networksize = htonl(size);

  DHTProtocol::set_src(rreq_p, src->_ether_addr.data());
  DHTProtocol::set_dst(rreq_p, dst->_ether_addr.data());

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rreq_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

WritablePacket *
DHTProtocolFalcon::fwd_nws_packet(DHTnode *src, DHTnode *next, uint32_t size, Packet *p)
{
  struct falcon_nws_packet *rreq = (struct falcon_nws_packet*)DHTProtocol::get_payload(p);

  rreq->networksize = htonl(size);
  DHTProtocol::set_dst(p, next->_ether_addr.data());

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(p->uniqueify(), &(src->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

void
DHTProtocolFalcon::get_nws_info(Packet *p, DHTnode *src, DHTnode *next, uint32_t *size)
{
  struct falcon_nws_packet *request = (struct falcon_nws_packet*)DHTProtocol::get_payload(p);

  *size = ntohl(request->networksize);

  src->set_update_addr(DHTProtocol::get_src_data(p));
  next->set_update_addr(DHTProtocol::get_dst_data(p));
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
ELEMENT_PROVIDES(DHTProtocolFalcon)
