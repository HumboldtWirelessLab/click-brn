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
 * dsrencap.{cc,hh} -- prepends a dsr header
 * A. Zubow
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "brn2_dsrprotocol.hh"
#include "brn2_dsrencap.hh"


CLICK_DECLS

BRN2DSREncap::BRN2DSREncap()
  : _link_table(),
    _me()
{
  BRNElement::init();
}

BRN2DSREncap::~BRN2DSREncap()
{
}

int
BRN2DSREncap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, /*"NodeIdentity",*/ &_me,
      "LINKTABLE", cpkP+cpkM, cpElement, /*"Link table",*/ &_link_table,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity"))
    return errh->error("NodeIdentity not specified");

  return 0;
}

int
BRN2DSREncap::initialize(ErrorHandler *)
{
  return 0;
}

/* creates a dsr source routed packet */
Packet *
BRN2DSREncap::add_src_header(Packet *p_in, EtherAddresses src_route)
{
  int i;
  int payload = sizeof(click_brn_dsr);
  int old_len = p_in->length();

  // the source and destination addresses are not included
  // as hops in the source route
  int route_len = src_route.size();
  assert(route_len >= 2);
  int hop_count = route_len - 2;

  BRN_DEBUG(" * creating DSR source-routed packet with payload %d", payload);

  payload += hop_count * sizeof(click_dsr_hop);

  // add the extra header size and get a new packet
  WritablePacket *p = p_in->push(payload);
  if (!p) {
    BRN_FATAL("couldn't add space for new DSR header.");
    return p;
  }

  // set brn dsr source route header
  click_brn_dsr *dsr_source = (click_brn_dsr *)(p->data());

  dsr_source->dsr_type = BRN_DSR_SRC;
  dsr_source->reserved = 0;
  dsr_source->dsr_id = 0;
  dsr_source->body.src.dsr_salvage = 7; // TODO change this !!
  dsr_source->dsr_segsleft = hop_count;
  dsr_source->dsr_hop_count = hop_count;

  memcpy(dsr_source->dsr_dst.data,
      (uint8_t *)src_route[0].data(), 6 * sizeof(uint8_t));
  memcpy(dsr_source->dsr_src.data,
      (uint8_t *)src_route[route_len - 1].data(), 6 * sizeof(uint8_t));

  // fetch IPs; TODO: dirty hack
  const click_ether *ether = (const click_ether *)p_in->data();
  if (ether->ether_type == ETHERTYPE_IP) { //TODO: XXXXXXXXXXXXXXXXXXXXX
    //const click_ip *iph = (const click_ip*)(p_in->data() + sizeof(click_ether));
    const click_ip *iph = (const click_ip *)p_in->ip_header();

    if (iph) {
      BRN_DEBUG(" * iph not NULL");
      IPAddress dst_ip_addr(iph->ip_dst);
      IPAddress src_ip_addr(iph->ip_src);

      dsr_source->dsr_ip_dst = dst_ip_addr.in_addr();
      dsr_source->dsr_ip_src = src_ip_addr.in_addr();

      BRN_DEBUG(" ** IP-Clients2 %s(%s) --> %s(%s)",
        src_route[route_len - 1].unparse().c_str(), src_ip_addr.unparse().c_str(),
        src_route[0].unparse().c_str(), dst_ip_addr.unparse().c_str());
    }
  }

  assert(hop_count < BRN_DSR_MAX_HOP_COUNT);

  BRN_DEBUG(_link_table->print_links().c_str());

  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(dsr_source);

  for (i = 0; i < hop_count; i++) {
    memcpy(dsr_hops[i].hw.data, (uint8_t *)src_route[hop_count - i].data(), 6 * sizeof(uint8_t));
    dsr_hops[i].metric = htons(0); // to be filled in along the way

    /*
     * If the src of the packet is a client station then the first hop is the meshnode this station
     * is associated with. So we need to set a proper metric between them. The metrics between the rest
     * of the nodes outlined within the route are updated along the way by forwarding nodes.
     */
    EtherAddress curr(dsr_hops[i].hw.data);
    if (_me->isIdentical(&curr) && (i == 0)) {
      EtherAddress prev(dsr_source->dsr_src.data);
      uint32_t last_hop_m = _link_table->get_link_metric(prev, curr);
      dsr_hops[i].metric = htons(last_hop_m);
      BRN_DEBUG(" * setting last hop metric: %s->%s=%d", prev.unparse().c_str(), curr.unparse().c_str(), last_hop_m);
    }
  }

  BRN_DEBUG(" * add_dsr_header: new packet size is %d, old was %d", p->length(), old_len);

  // copy packet destination annotation from incoming packet
  BRNPacketAnno::set_devicenumber_anno(p,(BRNPacketAnno::devicenumber_anno(p_in)));

  // set destination anno
  if (hop_count > 0) { // next hop on tour
    EtherAddress next_hop(src_route[route_len - 2].data());
  } else { // final destination is the last hop
    EtherAddress next_hop(src_route[0].data());
  }
  BRN_DEBUG(" * testing next hop %s", next_hop.unparse().c_str());
  BRNPacketAnno::set_dst_ether_anno(p,next_hop); //think about this
  BRNPacketAnno::set_ethertype_anno(p,ETHERTYPE_BRN);

  //TODO: remove the next stuff
  click_ether *cether = (click_ether *)p->ether_header();
  if ( cether != NULL ) {
    memcpy(cether->ether_dhost,(BRNPacketAnno::dst_ether_anno(p)).data() , 6);
  }

  return p;
}

/* creates a dsr route request packet with given src, dest and rreq_id */
/** TODO:
maybe reserve space for several hops */
Packet *
BRN2DSREncap::create_rreq(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip, uint16_t rreq_id)
{
  BRN_DEBUG(" * creating rreq: ... ");

  int payload = sizeof(click_brn_dsr); // rreq
  WritablePacket *p = WritablePacket::make(128, NULL, payload, 32); //Packet::make(payload);
  memset(p->data(), '\0', p->length());

  click_brn_dsr *dsr_rreq = (click_brn_dsr*)p->data();

  dsr_rreq->dsr_type = BRN_DSR_RREQ;
  dsr_rreq->dsr_id = htons(rreq_id);
  dsr_rreq->dsr_hop_count = 0;
  dsr_rreq->dsr_segsleft = 0;

  // real final destination
  memcpy(dsr_rreq->dsr_dst.data, (uint8_t *)dst.data(), 6 * sizeof(uint8_t));
  // ip addr
  dsr_rreq->dsr_ip_dst = dst_ip.in_addr();

  // set the initiator of the route request (here: an associated station)
  memcpy(dsr_rreq->dsr_src.data, (uint8_t *)src.data(), 6 * sizeof(uint8_t)); 
  // ip addr
  dsr_rreq->dsr_ip_src = src_ip.in_addr();

  return p;
}

/* creates a route reply packet. */
Packet *
BRN2DSREncap::create_rrep(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip,
  const BRN2RouteQuerierRoute &reply_route, uint16_t rreq_id)
{
  int i;
  // however the reply route has to include the destination.
  unsigned char reply_hop_count = reply_route.size() - 2;

  // creating the payload
  int payload = sizeof(click_brn_dsr);

  payload += reply_hop_count * sizeof(click_dsr_hop);
  // construct new packet
  WritablePacket *p = WritablePacket::make(128, NULL, payload, 32); //Packet::make(payload);

  if (!p) {
    BRN_FATAL(" * issue_rrep: couldn't make packet of %d bytes", payload);
    return 0;
  }
  
  memset(p->data(), '\0', p->length());

  // getting pointers to the headers
  click_brn_dsr *dsr = (click_brn_dsr *)p->data();

  // dsr header - fill the route reply header
  dsr->dsr_type = BRN_DSR_RREP;
  dsr->body.rrep.dsr_flags = 0;
  dsr->dsr_id = htons(rreq_id);
  //  dsr_rrep->dsr_id = htons(id); // TODO think about this !!!

  // source of rrep
  memcpy(dsr->dsr_src.data, (uint8_t *)src.data(), 6 * sizeof(uint8_t));
  // ip addr
  dsr->dsr_ip_src = src_ip.in_addr();

  // destination for rrep
  memcpy(dsr->dsr_dst.data, (uint8_t *)dst.data(), 6 * sizeof(uint8_t));
  // ip addr
  dsr->dsr_ip_dst = dst_ip.in_addr();

  // set hop count
  dsr->dsr_hop_count = reply_hop_count;

  int segments = reply_hop_count - 1;
  // starting from reply hop count
  dsr->dsr_segsleft = segments;

  // this is the index of the address to which this packet should be forwarded
  int index = reply_hop_count - segments;

  BRN_DEBUG(" * issue_rrep: filling hops %d", reply_route.size());
  // fill the route reply addrs
  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(dsr);//RobAt:DSR

  for (i = 0; i < reply_hop_count; i++) {
    // skip first address, which goes in the dest field of the dsr_dst header.
    memcpy(dsr_hops[i].hw.data, (uint8_t *)reply_route[i + 1].ether().data(),  6 * sizeof(uint8_t));
    dsr_hops[i].metric = htons(reply_route[i + 1]._metric); //TODO
  }

  // set destination anno
  if (reply_hop_count > 0) { // next hop on tour
    BRNPacketAnno::set_src_ether_anno(p,EtherAddress(dsr->dsr_src.data));
    BRNPacketAnno::set_dst_ether_anno(p,EtherAddress(dsr_hops[index].hw.data));
    BRNPacketAnno::set_ethertype_anno(p,ETHERTYPE_BRN);
  } else { // final destination is the last hop
    BRNPacketAnno::set_dst_ether_anno(p,EtherAddress(dst.data()));  BRNPacketAnno::set_ethertype_anno(p,ETHERTYPE_BRN);
  }

  BRN_DEBUG(" * next hop: %s", EtherAddress(dsr_hops[index].hw.data).unparse().c_str());
  BRN_DEBUG(" * issue_rrep: filling hops done , index= %d", index);

  return p;
}

/*
 * takes info about a bad link from bad_src -> bad_dst, and the
 * originator of the packet which failed on this link, and sends a
 * route error along the provided source_route
 */
Packet *
BRN2DSREncap::create_rerr(EtherAddress bad_src, EtherAddress bad_dst,
                          EtherAddress src, const BRN2RouteQuerierRoute &source_route)
{
  BRN_DEBUG(" * BRN2DSREncap::issue_rerr()");

  int i;
  // exclude src and dst in source route for hop count
  int src_hop_count = source_route.size() - 2;
  assert(src_hop_count >= 0);

  // creating the payload
  int payload = sizeof(click_brn_dsr);

  payload += src_hop_count * sizeof(click_dsr_hop);
  // make the packet
  WritablePacket *p = WritablePacket::make(128, NULL, payload, 32); //Packet::make(payload);

  if (!p) {
    BRN_FATAL(" * issue_rerr:  couldn't make packet of %d bytes", payload);
    return 0;
  }
  memset(p->data(), '\0', p->length());

  // getting pointers to the header
  click_brn_dsr *dsr_rerr = (click_brn_dsr *)p->data();

  dsr_rerr->dsr_type = BRN_DSR_RERR;
  dsr_rerr->dsr_hop_count = src_hop_count;

  // real final destination
  memcpy(dsr_rerr->dsr_dst.data, (uint8_t *)src.data(), 6 * sizeof(uint8_t));
  // set the initiator of the route request
  memcpy(dsr_rerr->dsr_src.data, (uint8_t *)bad_src.data(), 6 * sizeof(uint8_t)); 

  dsr_rerr->body.rerr.dsr_error = BRN_DSR_RERR_TYPE_NODE_UNREACHABLE;
  dsr_rerr->body.rerr.dsr_flags = 0;

  memcpy(dsr_rerr->body.rerr.dsr_unreachable_src.data, bad_src.data(),  6*sizeof(uint8_t));
  memcpy(dsr_rerr->body.rerr.dsr_unreachable_dst.data, bad_dst.data(),  6*sizeof(uint8_t));
  memcpy(dsr_rerr->body.rerr.dsr_msg_originator.data, src.data(),  6*sizeof(uint8_t));

  // fill in the source route
  dsr_rerr->dsr_segsleft = 0;
  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(dsr_rerr);//RobAt:DSR

  for (i = 0; i < src_hop_count; i++) {
    BRN_DEBUG(" * hop in err lst: %s", source_route[i + 1].ether_address.unparse().c_str());
    memcpy(dsr_hops[i].hw.data, (uint8_t *)source_route[i + 1].ether_address.data(), 6 * sizeof(uint8_t));
    //dsr_hops[i-1].metric = 0; //TODO think about Metric
  }

  // set hop count
  dsr_rerr->dsr_hop_count = src_hop_count;

  int segments = src_hop_count;
  // starting from reply hop count
  dsr_rerr->dsr_segsleft = segments;

  // this is the index of the address to which this packet should be forwarded
  int index = src_hop_count - segments;

  // set destination anno
  if (src_hop_count > 0) { // next hop on tour
    BRN_DEBUG(" * set dst anno: %s, index %d", EtherAddress(dsr_hops[index].hw.data).unparse().c_str(), index);
    BRNPacketAnno::set_dst_ether_anno(p,EtherAddress(dsr_hops[index].hw.data));
    BRNPacketAnno::set_ethertype_anno(p,ETHERTYPE_BRN);
  } else { // final destination is the last hop
    BRNPacketAnno::set_dst_ether_anno(p,EtherAddress(src.data()));
    BRNPacketAnno::set_ethertype_anno(p,ETHERTYPE_BRN);
  }

  BRN_DEBUG(" * next hop: %s", EtherAddress(dsr_hops[index].hw.data).unparse().c_str());
  return p;
}

Packet *
BRN2DSREncap::skipInMemoryHops(Packet *p_in)
{
  BRN_DEBUG(" * calling NodeIdentity::skipInMemoryHops().");

  click_brn_dsr *brn_dsr =
      (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

  int index = brn_dsr->dsr_hop_count - brn_dsr->dsr_segsleft;

  BRN_DEBUG(" * index = %d brn_dsr->dsr_hop_count = %d", index, brn_dsr->dsr_hop_count);

  if ( (brn_dsr->dsr_hop_count == 0) || (brn_dsr->dsr_segsleft == 0) )
    return p_in;

  assert(index >= 0 && index < BRN_DSR_MAX_HOP_COUNT);
  assert(index <= brn_dsr->dsr_hop_count);

  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(brn_dsr);//RobAt:DSR

  EtherAddress dest(dsr_hops[index].hw.data);

  BRN_DEBUG(" * test next hop: %s", dest.unparse().c_str());
  BRN_DEBUG(" * HC, Index, SL. %d %d %d", brn_dsr->dsr_hop_count, index, brn_dsr->dsr_segsleft);

  while (_me->isIdentical(&dest) && (brn_dsr->dsr_segsleft > 0)) {
    BRN_DEBUG(" * skip next hop: %s", dest.unparse().c_str());
    brn_dsr->dsr_segsleft--;
    index = brn_dsr->dsr_hop_count - brn_dsr->dsr_segsleft;

    dest = EtherAddress(dsr_hops[index].hw.data);
    BRN_DEBUG(" * check next hop (maybe skip required): %s", dest.unparse().c_str());
  }

  if (index == brn_dsr->dsr_hop_count) {// no hops left; use final dst
    BRN_DEBUG(" * using final dst. %d %d", brn_dsr->dsr_hop_count, index);
    BRNPacketAnno::set_dst_ether_anno(p_in,EtherAddress(brn_dsr->dsr_dst.data));
    BRNPacketAnno::set_ethertype_anno(p_in,ETHERTYPE_BRN);
  } else {
    BRNPacketAnno::set_dst_ether_anno(p_in,EtherAddress(dsr_hops[index].hw.data));
    BRNPacketAnno::set_ethertype_anno(p_in,ETHERTYPE_BRN);
  }

  return p_in;
}

Packet *
BRN2DSREncap::set_packet_to_next_hop(Packet * p_in)
{
  BRN_DEBUG(" * set unicast packet to next hop: ...");

  // create a writeable packet
  WritablePacket *p = p_in->uniqueify();

  click_brn_dsr *brn_dsr =
        (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

  if ( (brn_dsr->dsr_type != BRN_DSR_RREP) && (brn_dsr->dsr_type != BRN_DSR_SRC) && (brn_dsr->dsr_type != BRN_DSR_RERR)) {
    BRN_ERROR(" * source route option not found where expected, type = %d", brn_dsr->dsr_type);
    p->kill();
    return 0;
  }

  // nothing to do if no segments are left
  if (brn_dsr->dsr_segsleft == 0)
    return p;

  // after we forward it there will be (segsleft-1) hops left; 
  brn_dsr->dsr_segsleft--;

  int source_hops = brn_dsr->dsr_hop_count;
  int segments = brn_dsr->dsr_segsleft;

  BRN_DEBUG("segments %02x, source_hops %02x", segments, source_hops);

  assert(segments <= source_hops);

  BRN_DEBUG(" * BRN2DSREncap::set_packet_to_next_hop(): from %s to %s",
      EtherAddress(brn_dsr->dsr_src.data).unparse().c_str(), EtherAddress(brn_dsr->dsr_dst.data).unparse().c_str());

  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(brn_dsr);//RobAt:DSR
  // this is the index of the address to which this packet should be forwarded
  // update link metric in src packet
  if (source_hops > 0) { // nothing todo for source_hops == 0 (src, dst)
    int metric;
    int index = source_hops - segments;
    EtherAddress me(dsr_hops[index - 1].hw.data);
    if (index < 2) { // use src addr
      EtherAddress prev_node(brn_dsr->dsr_src.data);
      metric = _link_table->get_link_metric(prev_node, me);
      BRN_DEBUG(" * updating metric in src_packet: %s -> %s with %d", prev_node.unparse().c_str(), me.unparse().c_str(), metric);
    } else {
      EtherAddress prev_node(dsr_hops[index - 2].hw.data);
      metric = _link_table->get_link_metric(prev_node, me);
      BRN_DEBUG(" * updating metric in src_packet: %s -> %s with %d", prev_node.unparse().c_str(), me.unparse().c_str(), metric);
    }
    dsr_hops[index - 1].metric = htons(metric);
  }

  // skip own addresses
  skipInMemoryHops(p);

  if (brn_dsr->dsr_segsleft == 0) {
    BRN_DEBUG(" * no segments in route available; use final dest as next hop addr.");
    BRNPacketAnno::set_dst_ether_anno(p,EtherAddress(brn_dsr->dsr_dst.data));
    BRNPacketAnno::set_ethertype_anno(p,ETHERTYPE_BRN);
  }

  return p;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BRN2DSREncap::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DSREncap)
