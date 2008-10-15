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

#include "dsrencap.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
CLICK_DECLS

DSREncap::DSREncap()
  : _debug(BrnLogger::DEFAULT),
  _link_table(),
  _me()
{
}

DSREncap::~DSREncap()
{
}

int
DSREncap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpElement, "NodeIdentity", &_me,
                  cpElement, "Link table", &_link_table,
		  cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("NodeIdentity"))
    return errh->error("NodeIdentity not specified");

  return 0;
}

int
DSREncap::initialize(ErrorHandler *errh)
{
  _link_table = _me->get_link_table();
  if (!_link_table || !_link_table->cast("BrnLinkTable")) 
    return errh->error("BRNLinkTable not specified");

  return 0;
}

/* creates a dsr source routed packet */
Packet *
DSREncap::add_src_header(Packet *p_in, EtherAddresses src_route)
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

  // add the extra header size and get a new packet
  WritablePacket *p = p_in->push(payload);
  if (!p) {
    BRN_FATAL("couldn't add space for new DSR header.");
    return p;
  }

  // set brn dsr source route header
  click_brn_dsr *dsr_source = (click_brn_dsr *)(p->data());

  dsr_source->dsr_type = BRN_DSR_SRC;
  dsr_source->reserved[0] = 0; dsr_source->reserved[1] = 0; dsr_source->reserved[2] = 0;
  dsr_source->body.src.dsr_salvage = 7; // TODO change this !!
  dsr_source->dsr_segsleft = hop_count;
  dsr_source->dsr_hop_count = hop_count;

  memcpy(dsr_source->dsr_dst.data,
      (uint8_t *)src_route[0].data(), 6 * sizeof(uint8_t));
  memcpy(dsr_source->dsr_src.data,
      (uint8_t *)src_route[route_len - 1].data(), 6 * sizeof(uint8_t));

//
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
        src_route[route_len - 1].s().c_str(), src_ip_addr.s().c_str(),
        src_route[0].s().c_str(), dst_ip_addr.s().c_str());
    }
  }
//
  assert(hop_count < BRN_DSR_MAX_HOP_COUNT);

  BRN_DEBUG(_link_table->print_links().c_str());

  for (i = 0; i < hop_count; i++) {
    memcpy(dsr_source->addr[i].hw.data, (uint8_t *)src_route[hop_count - i].data(), 6 * sizeof(uint8_t));
    dsr_source->addr[i].metric = htons(0); // to be filled in along the way

    /*
     * If the src of the packet is a client station then the first hop is the meshnode this station
     * is associated with. So we need to set a proper metric between them. The metrics between the rest
     * of the nodes outlined within the route are updated along the way by forwarding nodes.
     */
    EtherAddress curr(dsr_source->addr[i].hw.data);
    if (_me->isIdentical(&curr) && (i == 0)) {
      EtherAddress prev(dsr_source->dsr_src.data);
      uint32_t last_hop_m = _link_table->get_link_metric(prev, curr);
      dsr_source->addr[i].metric = htons(last_hop_m);
      BRN_DEBUG(" * setting last hop metric: %s->%s=%d", prev.s().c_str(), curr.s().c_str(), last_hop_m);
    }
  }

  BRN_DEBUG(" * add_dsr_header: new packet size is %d, old was %d", p->length(), old_len);

  // copy packet destination annotation from incoming packet
//BRNNEW  p->set_udevice_anno(p_in->udevice_anno().c_str());

  // set destination anno
  if (hop_count > 0) { // next hop on tour
    EtherAddress next_hop(src_route[route_len - 2].data());
    BRN_DEBUG(" * testing next hop %s", next_hop.s().c_str());
//BRNNEW    p->set_dst_ether_anno(next_hop); //think about this
  } else { // final destination is the last hop
    EtherAddress next_hop(src_route[0].data());
    BRN_DEBUG(" * next hop %s", next_hop.s().c_str());
 //BRNNEW   p->set_dst_ether_anno(next_hop); //think about this
  }
  return p;
}

/* creates a dsr route request packet with given src, dest and rreq_id */
Packet *
DSREncap::create_rreq(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip, uint16_t rreq_id)
{
  BRN_DEBUG(" * creating rreq: ... ");

  int payload = sizeof(click_brn_dsr); // rreq
  WritablePacket *p = Packet::make(payload);
  memset(p->data(), '\0', p->length());

  click_brn_dsr *dsr_rreq = (click_brn_dsr*)p->data();

  dsr_rreq->dsr_type = BRN_DSR_RREQ;
  dsr_rreq->body.rreq.dsr_id = htons(rreq_id); 
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
DSREncap::create_rrep(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip,
  const RouteQuerierRoute &reply_route, uint16_t rreq_id)
{
  int i;
  // however the reply route has to include the destination.
  unsigned char reply_hop_count = reply_route.size() - 2;

  // creating the payload
  int payload = sizeof(click_brn_dsr);

  // construct new packet
  WritablePacket *p = Packet::make(payload);
  memset(p->data(), '\0', p->length());

  if (!p) {
    BRN_FATAL(" * issue_rrep: couldn't make packet of %d bytes", payload);
    return 0;
  }

  // getting pointers to the headers
  click_brn_dsr *dsr = (click_brn_dsr *)p->data();

  // dsr header - fill the route reply header
  dsr->dsr_type = BRN_DSR_RREP;
  dsr->body.rrep.dsr_flags = 0;
  dsr->body.rreq.dsr_id = htons(rreq_id);
  //  dsr_rrep->dsr_id = htons(id); // TODO think about this !!!

  // source of rrep
  memcpy(dsr->dsr_src.data, (uint8_t *)src.data(), 6 * sizeof(uint8_t));
  // ip addr
  dsr->dsr_ip_src = src_ip.in_addr();

  // destination for rrep
  memcpy(dsr->dsr_dst.data, (uint8_t *)dst.data(), 6 * sizeof(uint8_t));
  // ip addr
  dsr->dsr_ip_dst = dst_ip.in_addr();

  BRN_DEBUG(" * issue_rrep: filling hops %d", reply_route.size());
  // fill the route reply addrs
  for (i = 0; i < reply_hop_count; i++) {
    // skip first address, which goes in the dest field of the dsr_dst header.
    memcpy(dsr->addr[i].hw.data,
        (uint8_t *)reply_route[i + 1].ether().data(),  6 * sizeof(uint8_t));
    dsr->addr[i].metric = htons(reply_route[i + 1]._metric); //TODO
  }

  // set hop count
  dsr->dsr_hop_count = reply_hop_count;

  int segments = reply_hop_count - 1;
  // starting from reply hop count
  dsr->dsr_segsleft = segments;

  // this is the index of the address to which this packet should be forwarded
  int index = reply_hop_count - segments;

  // set destination anno
/*BRNNEW  if (reply_hop_count > 0) { // next hop on tour
    p->set_dst_ether_anno(EtherAddress(dsr->addr[index].hw.data));
  } else { // final destination is the last hop
    p->set_dst_ether_anno(EtherAddress(dst.data()));
  }
*/
  BRN_DEBUG(" * next hop: %s", EtherAddress(dsr->addr[index].hw.data).s().c_str());
  BRN_DEBUG(" * issue_rrep: filling hops done , index= %d", index);

  return p;
}

/*
 * takes info about a bad link from bad_src -> bad_dst, and the
 * originator of the packet which failed on this link, and sends a
 * route error along the provided source_route
 */
Packet *
DSREncap::create_rerr(EtherAddress bad_src, EtherAddress bad_dst,
                          EtherAddress src, const RouteQuerierRoute &source_route)
{
  BRN_DEBUG(" * DSREncap::issue_rerr()");

  int i;
  // exclude src and dst in source route for hop count
  int src_hop_count = source_route.size() - 2;
  assert(src_hop_count >= 0);

  // creating the payload
  int payload = sizeof(click_brn_dsr);

  // make the packet
  WritablePacket *p = Packet::make(payload);
  memset(p->data(), '\0', p->length());

  if (!p) {
    BRN_FATAL(" * issue_rerr:  couldn't make packet of %d bytes", payload);
    return 0;
  }

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
  for (i = 0; i < src_hop_count; i++) {
    BRN_DEBUG(" * hop in err lst: %s", source_route[i + 1].ether_address.s().c_str());
    memcpy(dsr_rerr->addr[i].hw.data, (uint8_t *)source_route[i + 1].ether_address.data(), 6 * sizeof(uint8_t));
    //dsr_rerr->addr[i-1].metric = 0; //TODO think about Metric
  }

  // set hop count
  dsr_rerr->dsr_hop_count = src_hop_count;

  int segments = src_hop_count;
  // starting from reply hop count
  dsr_rerr->dsr_segsleft = segments;

  // this is the index of the address to which this packet should be forwarded
  int index = src_hop_count - segments;

  // set destination anno
/*BRNNEW  if (src_hop_count > 0) { // next hop on tour
    BRN_DEBUG(" * set dst anno: %s, index %d", EtherAddress(dsr_rerr->addr[index].hw.data).s().c_str(), index);
    p->set_dst_ether_anno(EtherAddress(dsr_rerr->addr[index].hw.data));
  } else { // final destination is the last hop
    p->set_dst_ether_anno(EtherAddress(src.data()));
  }
*/
  BRN_DEBUG(" * next hop: %s", EtherAddress(dsr_rerr->addr[index].hw.data).s().c_str());
  return p;
}

Packet *
DSREncap::set_packet_to_next_hop(Packet * p_in)
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

  BRN_DEBUG(" * DSREncap::set_packet_to_next_hop(): from %s to %s",
      EtherAddress(brn_dsr->dsr_src.data).s().c_str(), EtherAddress(brn_dsr->dsr_dst.data).s().c_str());

  // this is the index of the address to which this packet should be forwarded
  // update link metric in src packet
  if (source_hops > 0) { // nothing todo for source_hops == 0 (src, dst)
    int metric;
    int index = source_hops - segments;
    EtherAddress me(brn_dsr->addr[index - 1].hw.data);
    if (index < 2) { // use src addr
      EtherAddress prev_node(brn_dsr->dsr_src.data);
      metric = _link_table->get_link_metric(prev_node, me);
      BRN_DEBUG(" * updating metric in src_packet: %s -> %s with %d", prev_node.s().c_str(), me.s().c_str(), metric);
    } else {
      EtherAddress prev_node(brn_dsr->addr[index - 2].hw.data);
      metric = _link_table->get_link_metric(prev_node, me);
      BRN_DEBUG(" * updating metric in src_packet: %s -> %s with %d", prev_node.s().c_str(), me.s().c_str(), metric);
    }
    brn_dsr->addr[index - 1].metric = htons(metric);
  }

  // skip own addresses
  _me->skipInMemoryHops(p);

/*BRNNEW  if (brn_dsr->dsr_segsleft == 0) {
    BRN_DEBUG(" * no segments in route available; use final dest as next hop addr.");
    p->set_dst_ether_anno(EtherAddress(brn_dsr->dsr_dst.data));
  }
*/
  return p;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  DSREncap *de = (DSREncap *)e;
  return String(de->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  DSREncap *de = (DSREncap *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  de->_debug = debug;
  return 0;
}

void
DSREncap::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DSREncap)
