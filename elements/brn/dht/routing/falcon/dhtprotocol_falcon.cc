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

/*********************************************************************************/
/****************************** L I N K P R O B E ********************************/
/*********************************************************************************/

int32_t
DHTProtocolFalcon::max_no_nodes_in_lp(int32_t buffer_len)
{
  return (buffer_len/sizeof(struct dht_falcon_node_entry))-1;
}

int32_t
DHTProtocolFalcon::pack_lp(uint8_t *buffer, int32_t buffer_len, DHTnode *me, DHTnodelist *nodes)
{
  if ( (unsigned)buffer_len < sizeof(struct dht_falcon_node_entry) ) return 0;

  struct dht_falcon_node_entry *ne = (struct dht_falcon_node_entry*)buffer;

  ne->age_sec = 0;
  ne->status = me->_status;
  memcpy(ne->etheraddr, me->_ether_addr.data(), 6);
  memcpy(ne->node_id, me->_md5_digest, sizeof(ne->node_id));

  int32_t buffer_left = buffer_len - sizeof(struct dht_falcon_node_entry);
  int32_t node_index = 0;

  if ( nodes != NULL ) {
    while ( (buffer_left >= (int)sizeof(struct dht_falcon_node_entry)) && ( node_index < nodes->size()) ) {
      DHTnode *ac_node = nodes->get_dhtnode(node_index);
      node_index++;
      ne[node_index].age_sec = ac_node->get_age_s();
      ne[node_index].status = ac_node->_status;
      memcpy(ne[node_index].etheraddr, ac_node->_ether_addr.data(), 6);
      memcpy(ne[node_index].node_id, ac_node->_md5_digest, sizeof(ne->node_id));
      buffer_left -= sizeof(struct dht_falcon_node_entry);
    }
  }

  return sizeof(dht_falcon_node_entry) * (node_index + 1);
}

int32_t
DHTProtocolFalcon::unpack_lp(uint8_t *buffer, int32_t buffer_len, DHTnode *first, DHTnodelist *nodes)
{
  struct dht_falcon_node_entry *ne = (struct dht_falcon_node_entry*)buffer;

  first->set_age_now();
  first->_status = ne->status;
  first->set_etheraddress(ne->etheraddr);
  first->set_nodeid(ne->node_id);

  int32_t buffer_left = sizeof(struct dht_falcon_node_entry);
  int32_t node_index = 0;

  if ( nodes != NULL ) {
    while ( buffer_left < buffer_len ) {
      node_index++;
      DHTnode *ac_node = new DHTnode(EtherAddress(ne[node_index].etheraddr),ne[node_index].node_id);
      ac_node->set_age_s(ne[node_index].age_sec);
      ac_node->_status = ne[node_index].status;
      nodes->add_dhtnode(ac_node);
      buffer_left += sizeof(struct dht_falcon_node_entry);
    }
  }

  return 0;
}

/*********************************************************************************/
/******************************* R O U T I N G  **********************************/
/*********************************************************************************/

WritablePacket *
DHTProtocolFalcon::new_route_request_packet(DHTnode *src, DHTnode *dst, uint8_t type, int request_position)
{
  WritablePacket *rreq_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, type, sizeof(struct falcon_routing_packet));
  struct falcon_routing_packet *request = (struct falcon_routing_packet*)DHTProtocol::get_payload(rreq_p);

  request->table_position = htons(request_position);

  DHTProtocol::set_src(rreq_p, src->_ether_addr.data());
  memcpy(request->src_node_id, src->_md5_digest, sizeof(request->src_node_id));
  request->src_status = src->_status;

  memcpy(request->etheraddr,dst->_ether_addr.data(),6);
  memcpy(request->node_id, dst->_md5_digest, sizeof(request->node_id));
  request->node_status = dst->_status;

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rreq_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

WritablePacket *
DHTProtocolFalcon::new_route_reply_packet(DHTnode *src, DHTnode *dst, uint8_t type, DHTnode *node, int request_position, Packet *p_recycle)
{
  WritablePacket *rrep_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, type, sizeof(struct falcon_routing_packet),p_recycle);
  struct falcon_routing_packet *reply = (struct falcon_routing_packet*)DHTProtocol::get_payload(rrep_p);

  reply->table_position = htons(request_position);

  DHTProtocol::set_src(rrep_p, src->_ether_addr.data());
  memcpy(reply->src_node_id, src->_md5_digest, sizeof(reply->src_node_id));
  reply->src_status = src->_status;

  memcpy(reply->etheraddr, node->_ether_addr.data(),6);
  memcpy(reply->node_id, node->_md5_digest, sizeof(reply->node_id));
  reply->node_status = node->_status;

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rrep_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

WritablePacket *
DHTProtocolFalcon::fwd_route_request_packet(DHTnode *src, DHTnode *new_dst, DHTnode *fwd, Packet *p)
{
  WritablePacket *rfwd_p = p->uniqueify();
  struct falcon_routing_packet *request = (struct falcon_routing_packet*)DHTProtocol::get_payload(rfwd_p);

  DHTProtocol::set_src(rfwd_p, src->_ether_addr.data());
  memcpy(request->src_node_id, src->_md5_digest, sizeof(request->src_node_id));
  request->src_status = src->_status;

  memcpy(request->etheraddr, new_dst->_ether_addr.data(),6);
  memcpy(request->node_id, new_dst->_md5_digest, sizeof(request->node_id));
  request->node_status = new_dst->_status;

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rfwd_p, &(fwd->_ether_addr), &(new_dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

void
DHTProtocolFalcon::get_info(Packet *p, DHTnode *src, DHTnode *node, uint16_t *pos)
{
  struct falcon_routing_packet *request = (struct falcon_routing_packet*)DHTProtocol::get_payload(p);

  *pos = ntohs(request->table_position);

  src->set_etheraddress(DHTProtocol::get_src_data(p));
  src->set_nodeid(request->src_node_id);
  src->set_age_now();
  src->_status = request->src_status;

  node->set_etheraddress(request->etheraddr);
  node->set_nodeid(request->node_id);
  node->_status = request->node_status;
}

/*********************************************************************************/
/*********************** R O U T I N G   L E A V E *******************************/
/*********************************************************************************/

/**
 * DHTnode *src
 * DHTnode *dst
 * uint8_t operation
 * DHTnode *node:        successor of node
 * int request_position
*/

WritablePacket *
DHTProtocolFalcon::new_route_leave_packet(DHTnode *src, DHTnode *dst, uint8_t operation, DHTnode *node, int request_position)
{
  WritablePacket *leave_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, operation, sizeof(struct falcon_routing_packet));
  struct falcon_routing_packet *leave = (struct falcon_routing_packet*)DHTProtocol::get_payload(leave_p);

  leave->table_position = htons(request_position);

  DHTProtocol::set_src(leave_p, src->_ether_addr.data());
  memcpy(leave->src_node_id, src->_md5_digest, sizeof(leave->src_node_id));
  leave->src_status = src->_status;

  memcpy(leave->etheraddr, node->_ether_addr.data(),6);
  memcpy(leave->node_id, node->_md5_digest, sizeof(leave->node_id));
  leave->node_status = node->_status;

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(leave_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

/*********************************************************************************/
/*************************** N E T W O R K S I Z E *******************************/
/*********************************************************************************/

WritablePacket *
DHTProtocolFalcon::new_nws_packet(DHTnode *src, DHTnode *dst, uint32_t size)
{
  WritablePacket *rreq_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, FALCON_MINOR_NWS_REQUEST, sizeof(struct falcon_nws_packet));
  struct falcon_nws_packet *request = (struct falcon_nws_packet*)DHTProtocol::get_payload(rreq_p);

  request->networksize = htonl(size);

  DHTProtocol::set_src(rreq_p, src->_ether_addr.data());
  memcpy(request->src_node_id, src->_md5_digest, sizeof(request->src_node_id));

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(rreq_p, &(src->_ether_addr), &(dst->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

WritablePacket *
DHTProtocolFalcon::fwd_nws_packet(DHTnode *src, DHTnode *next, uint32_t size, Packet *p)
{
  struct falcon_nws_packet *rreq = (struct falcon_nws_packet*)DHTProtocol::get_payload(p);

  rreq->networksize = htonl(size);

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(p->uniqueify(), &(src->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTROUTING);

  return(brn_p);
}

void
DHTProtocolFalcon::get_nws_info(Packet *p, DHTnode *src, uint32_t *size)
{
  struct falcon_nws_packet *request = (struct falcon_nws_packet*)DHTProtocol::get_payload(p);

  *size = ntohl(request->networksize);

  src->_status = STATUS_OK;
  src->set_etheraddress(DHTProtocol::get_src_data(p));
  src->set_nodeid(request->src_node_id);
  src->set_age_now();
}

/*********************************************************************************/
/******************** P A S S I V E   M O N I T O R I N G ************************/
/*********************************************************************************/

WritablePacket *
DHTProtocolFalcon::new_passive_monitor_active_packet(DHTnode *src, EtherAddress *dst, DHTnodelist *reverse_fingertable)
{
  WritablePacket *act_p = DHTProtocol::new_dht_packet(ROUTING_FALCON, FALCON_MINOR_PASSIVE_MONITORING_ACTIVATE,
                                                       sizeof(struct falcon_passiv_monitoring_info) +
                                      reverse_fingertable->size() * sizeof(struct dht_falcon_reverse_table_node_entry));

  struct falcon_passiv_monitoring_info *fpmi = (struct falcon_passiv_monitoring_info*)DHTProtocol::get_payload(act_p);

  uint8_t res = 0;
  fpmi->status = 0;
  fpmi->no_nodes = reverse_fingertable->size();
  src->get_nodeid((md5_byte_t *)fpmi->passive_node_id, &res);

  struct dht_falcon_reverse_table_node_entry *node_tab = (struct dht_falcon_reverse_table_node_entry *)&fpmi[1];

  for ( int i = 0; i < reverse_fingertable->size(); i++ ) {
    memcpy( node_tab[i].etheraddr, reverse_fingertable->get_dhtnode(i)->_ether_addr.data(),6);
    memcpy(node_tab[i].node_id, reverse_fingertable->get_dhtnode(i)->_md5_digest,MAX_NODEID_LENTGH);
  }

  WritablePacket *brn_p = DHTProtocol::push_brn_ether_header(act_p, &(src->_ether_addr), dst, BRN_PORT_DHTROUTING);

  return brn_p;
}

WritablePacket *
DHTProtocolFalcon::new_passive_monitor_deactive_packet(DHTnode */*src*/)
{
  return NULL;
}

WritablePacket *
DHTProtocolFalcon::new_passive_monitor_leave_notification_packet(DHTnode */*src*/, DHTnode */*dst*/, DHTnode */*leave_node*/)
{
  return NULL;
}

WritablePacket *
DHTProtocolFalcon::new_passive_monitor_leave_reply_packet(DHTnode */*src*/, DHTnode */*dst*/, DHTnode */*leave_node*/)
{
  return NULL;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
ELEMENT_PROVIDES(DHTProtocolFalcon)
