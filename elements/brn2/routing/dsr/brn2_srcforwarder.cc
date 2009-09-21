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
 * srcforwarder.{cc,hh} -- forwards dsr source routed packets
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brn2_logger.hh"

#include "brn2_dsrencap.hh"
#include "brn2_dsrdecap.hh"
#include "brn2_routequerier.hh"

#include "brn2_srcforwarder.hh"

CLICK_DECLS

BRN2SrcForwarder::BRN2SrcForwarder()
  : _debug(Brn2Logger::DEFAULT),
  _me(),
  _dsr_encap(),
  _dsr_decap(),
  _link_table()
{
  //add_input(); // previously generated src packet needs to be forwarded
  //add_input(); // receiving src packet (dest reached or packet has to be forwarded)

  //add_output(); // source routed packet has to be forwarded
  //add_output(); // src packet reached final destination (internal node or client)
  //add_output(); // src packet reached final destination (external node - client)
}

BRN2SrcForwarder::~BRN2SrcForwarder()
{
}

int
BRN2SrcForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      "DSRENCAP", cpkP+cpkM, cpElement, &_dsr_encap,
      "DSRDECAP", cpkP+cpkM, cpElement, &_dsr_decap,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_dsr_encap || !_dsr_encap->cast("BRN2DSREncap"))
    return errh->error("DSREncap not specified");

  if (!_dsr_decap || !_dsr_decap->cast("BRN2DSRDecap")) 
    return errh->error("DSRDecap not specified");

  return 0;
}

int
BRN2SrcForwarder::initialize(ErrorHandler *)
{
  return 0;
}

void
BRN2SrcForwarder::uninitialize()
{
  //cleanup
}

Packet *
BRN2SrcForwarder::skipInMemoryHops(Packet *p_in)
{
  BRN_DEBUG(" * calling NodeIdentity::skipInMemoryHops().");

  click_brn_dsr *brn_dsr = (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

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

void
BRN2SrcForwarder::push(int port, Packet *p_in)
{
  BRN_DEBUG("push()");

  if (port == 0) {  // previously generated src packet needs to be forwarded
    Packet *p_out = skipInMemoryHops(p_in);
    output(0).push(p_out);
  } else if (port == 1) { // src packets received by this node
    BRN_DEBUG(" * source routed packet received from other node.");

    click_brn_dsr *brn_dsr = (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

    assert(brn_dsr->dsr_type == BRN_DSR_SRC);

    // fetch the final destination of dsr source packet
    EtherAddress dst_addr(brn_dsr->dsr_dst.data);

    // learn from this packet
    BRN2RouteQuerierRoute source_route;
    _dsr_decap->extract_source_route(p_in, source_route);

    if (_debug == Brn2Logger::DEBUG) {
      BRN_DEBUG(" * learned from SRCPacket ...");
      for (int j = 0; j < source_route.size(); j++)
        BRN_DEBUG(" SRC - %d   %s (%d)",
          j, source_route[j].ether().unparse().c_str(), source_route[j]._metric);
    }

    // update link table
    add_route_to_link_table(source_route);

    //BRN_DEBUG(_link_table->print_links().c_str());

    if (_me->isIdentical(&dst_addr) || ( _link_table->get_host_metric_to_me(dst_addr) == 50 )) { // for me
      BRN_DEBUG(" * source routed packet reached final destination (me or my assoc clients)");

      // remove all brn header and return packet to kernel
      Packet *p = strip_all_headers(p_in);
      output(1).push(p);
    } else {
      BRN_DEBUG(" * need to forward to %s", dst_addr.unparse().c_str());
      // determines next hop
      forward_data(p_in); //use port 0
    }
  } else {
    BRN_FATAL(" Wrong port.");
  }
}

void
BRN2SrcForwarder::forward_data(Packet *p_in)
{
  BRN_DEBUG("* forward_src: ...");

  Packet *p_out = _dsr_encap->set_packet_to_next_hop(p_in);

  // check the link metric from me to the dst
  /*const*/ click_brn_dsr *brn_dsr = //RobAt:DSR
        (/*const*/  click_brn_dsr *)(p_out->data() + sizeof(click_brn));

  int source_hops  = brn_dsr->dsr_hop_count;
  int segments     = brn_dsr->dsr_segsleft;

  // nothing todo for source_hops == 0 (src, dst)
  assert(source_hops > 0);
  assert(segments >= 0);

  int index = source_hops - segments;

  EtherAddress me(brn_dsr->dsr_src.data);
  EtherAddress next(brn_dsr->dsr_dst.data);

  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(brn_dsr);//RobAt:DSR

  if (segments < source_hops) {
    // intermediate hop
    me = EtherAddress (dsr_hops[index - 1].hw.data);
  } else {
    // first hop
  }

  if (segments > 0) {
    // NOT the last hop
    next = EtherAddress (dsr_hops[index].hw.data);
  } else {
    // last hop
  }

  // Check if the next hop exists in link table    
  if (BRN_DSR_INVALID_ROUTE_METRIC == _link_table->get_link_metric(me, next)) {
    BRN_DEBUG(" * BRN2SrcForwarder: no link between %s and %s, broken source route.", me.unparse().c_str(), next.unparse().c_str());
    BRN_DEBUG(_link_table->print_links().c_str());

    WritablePacket* p = p_in->uniqueify();
    click_ether *ether = (click_ether *)p->ether_header();

    memcpy(ether->ether_shost, me.data(), sizeof(ether->ether_shost));
    memcpy(ether->ether_dhost, next.data(), sizeof(ether->ether_dhost));
    ether->ether_type = htons(ETHERTYPE_BRN);                                         //TODO: CHECK: this is important ??

    // the following line would effectively destroy p_in as it creates a shared ether header between p and p_in
    // Yet, there is no obvious reason to have it around at all. We shouldn't touch p_in here.
    //p_in->set_ether_header(ether);

    output(2).push(p);  //Route-Error
    return;
  }

  click_ether *ether = (click_ether *)p_in->ether_header();//TODO: CHECK: this is important ??
  memcpy(ether->ether_shost, me.data(), sizeof(ether->ether_shost));  //TODO: copx
  memcpy(ether->ether_dhost, next.data(), sizeof(ether->ether_dhost));  //TODO: is a copy
  ether->ether_type = htons(ETHERTYPE_BRN);                //TODO: CHECK: this is important ??
  // ouput packet
  output(0).push(p_out);
}

Packet *
BRN2SrcForwarder::strip_all_headers(Packet *p_in)
{
  click_brn_dsr *dsr_src = (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

  //int old_len = p_in->length();
  int brn_dsr_len = sizeof(click_brn) + DSRProtocol::header_length(dsr_src);

//  uint8_t type = dsr_src->body.src.dsr_payload_type;
  EtherAddress src(dsr_src->dsr_src.data);
  EtherAddress dst(dsr_src->dsr_dst.data);

  WritablePacket *p = p_in->uniqueify();
  //click_ether *ether = (click_ether *)(p->data() + brn_dsr_len);

  // remove the headers  
  p->pull(brn_dsr_len);
  //memcpy(p->data(), ether, (old_len - brn_dsr_len));

  // set dsr payload type
  //p->set_user_anno_c(BRN_DSR_PAYLOADTYPE_KEY, type);

  // ether anno
  click_ether* ether_anno = const_cast<click_ether*>(p->ether_header());
  if( NULL == ether_anno ) {
    p = p->push_mac_header(sizeof(click_ether));
    ether_anno = const_cast<click_ether*>(p->ether_header());
    //TODO: ether type cannot be set here.
    p->pull(sizeof(click_ether));
  }

  memcpy( ether_anno->ether_shost, src.data(), 6 );
  memcpy( ether_anno->ether_dhost, dst.data(), 6 );

  BRN_DEBUG(" * stripping headers; removed %d bytes", brn_dsr_len);

  return p;
}

void
BRN2SrcForwarder::add_route_to_link_table(const BRN2RouteQuerierRoute &route)
{

  for (int i = 0; i < route.size() - 1; i++) {
    EtherAddress ether1 = route[i].ether();
    EtherAddress ether2 = route[i+1].ether();

    if (ether1 == ether2)
      continue;

    if (_me->isIdentical(&ether2)) // learn only from route prefix; suffix will be set along the way
      return;

    uint16_t metric = route[i+1]._metric; //metric starts with offset 1

    IPAddress ip1 = route[i].ip();
    IPAddress ip2 = route[i+1].ip();

/*
    if (metric == BRN_DSR_INVALID_HOP_METRIC) {
      metric = 9999;
    }
    if (metric == 0) {
      metric = 1; // TODO remove this hack
    }
*/
    bool ret = _link_table->update_both_links(ether1, ip1, ether2, ip2, 0, 0, metric);

    if (ret) {
      BRN_DEBUG(" _link_table->update_link %s (%s) %s (%s) %d",
        route[i].ether().unparse().c_str(), route[i].ip().unparse().c_str(),
        route[i+1].ether().unparse().c_str(), route[i+1].ip().unparse().c_str(), metric);
    }
  }
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRN2SrcForwarder *sf = (BRN2SrcForwarder *)e;
  return String(sf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRN2SrcForwarder *sf = (BRN2SrcForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  sf->_debug = debug;
  return 0;
}

void
BRN2SrcForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2SrcForwarder)
