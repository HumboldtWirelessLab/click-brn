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

#include "elements/brn/brnprotocol/brnprotocol.hh"

#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"
#include "elements/brn/dht/protocol/dhtprotocol.hh"
#include "dhtprotocol_dart.hh"

CLICK_DECLS

int
DHTProtocolDart::pack_lp(uint8_t *buffer, int /*buffer_len*/, DHTnode *me, DHTnodelist */*nodes*/)
{
  struct dht_dart_lp_node_entry *ne = (struct dht_dart_lp_node_entry*)buffer;
  ne->status = me->_status;
  ne->id_size = me->_digest_length;
  memcpy(ne->etheraddr, me->_ether_addr.data(), 6);
  memcpy(ne->id, me->_md5_digest, MAX_NODEID_LENTGH);

  return sizeof(dht_dart_lp_node_entry);
}

int
DHTProtocolDart::unpack_lp(uint8_t *buffer, int /*buffer_len*/, DHTnode *first, DHTnodelist */*nodes*/)
{
  struct dht_dart_lp_node_entry *ne = (struct dht_dart_lp_node_entry*)buffer;

  first->_age = Timestamp::now();
  first->_status = ne->status;
  first->set_update_addr(ne->etheraddr);
  first->set_nodeid(ne->id, ne->id_size);

  return 0;
}

WritablePacket *
DHTProtocolDart::new_dart_nodeid_packet( DHTnode *src, DHTnode *dst, int type, Packet *p)
{
  WritablePacket *nid_p;

  if ( p != NULL ) {
    nid_p = p->uniqueify();
    DHTProtocol::set_type(nid_p,type);
  } else
    nid_p = DHTProtocol::new_dht_packet(ROUTING_DART, type, sizeof(struct dht_dart_routing));

  struct dht_dart_routing *request = (struct dht_dart_routing*)DHTProtocol::get_payload(nid_p);

  request->status = 0;

  request->src_id_size = src->_digest_length;
  memcpy(request->src_id, src->_md5_digest, MAX_NODEID_LENTGH);

  request->dst_id_size = dst->_digest_length;
  memcpy(request->dst_id, dst->_md5_digest, MAX_NODEID_LENTGH); ;

  DHTProtocol::set_src(nid_p, src->_ether_addr.data());

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(nid_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}


WritablePacket *
DHTProtocolDart::new_nodeid_request_packet( DHTnode *src, DHTnode *dst)
{
  return new_dart_nodeid_packet( src, dst, DART_MINOR_REQUEST_ID, NULL);
}

WritablePacket *
DHTProtocolDart::new_nodeid_assign_packet( DHTnode *src, DHTnode *dst, Packet *p)
{
  return new_dart_nodeid_packet( src, dst, DART_MINOR_ASSIGN_ID, p);
}

void
DHTProtocolDart::get_info(Packet *p, DHTnode *src, DHTnode *node, uint8_t *status)
{
  struct dht_dart_routing *request = (struct dht_dart_routing*)DHTProtocol::get_payload(p);

  *status = request->status;

  src->set_update_addr(DHTProtocol::get_src_data(p));
  src->set_nodeid(request->src_id,request->src_id_size);

  node->set_nodeid(request->dst_id,request->dst_id_size);
}

void
DHTProtocolDart::get_info(Packet *p, DHTnode *src, uint8_t *status)
{
  struct dht_dart_routing *request = (struct dht_dart_routing*)DHTProtocol::get_payload(p);

  *status = request->status;

  src->set_update_addr(DHTProtocol::get_src_data(p));
  src->set_nodeid(request->src_id,request->src_id_size);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
ELEMENT_PROVIDES(DHTProtocolDart)
