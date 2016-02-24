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

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brn2_dsrencap.hh"
#include "brn2_dsrdecap.hh"
#include "brn2_routequerier.hh"

#include "brn2_srcforwarder.hh"

CLICK_DECLS

BRN2SrcForwarder::BRN2SrcForwarder()
  : _me(),
    _dsr_encap(),
    _dsr_decap(),
    _link_table(),
    _route_querier(),
    _dsr_rid_cache(NULL)
{
  BRNElement::init();
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
      "ROUTEQUERIER", cpkP+cpkM, cpElement, &_route_querier,
      "DSRIDCACHE", cpkP, cpElement, &_dsr_rid_cache,
      "DEBUG", cpkP, cpInteger, &_debug,
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
  WritablePacket *p_out = p_in->uniqueify();

  BRN_DEBUG(" * calling NodeIdentity::skipInMemoryHops().");

  click_brn_dsr *brn_dsr = reinterpret_cast<click_brn_dsr *>((p_out->data() + sizeof(click_brn)));

  int index = brn_dsr->dsr_hop_count - brn_dsr->dsr_segsleft;

  BRN_DEBUG(" * index = %d brn_dsr->dsr_hop_count = %d", index, brn_dsr->dsr_hop_count);

  if ( (brn_dsr->dsr_hop_count == 0) || (brn_dsr->dsr_segsleft == 0) )
    return p_out;

  assert(index >= 0 && index < BRN_DSR_MAX_HOP_COUNT);
  assert(index <= brn_dsr->dsr_hop_count);

  const click_dsr_hop *dsr_hops = DSRProtocol::get_hops(brn_dsr);//RobAt:DSR

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

  EtherAddress src(brn_dsr->dsr_src.data);
  if ( index > 0 )
    src = EtherAddress(dsr_hops[index - 1].hw.data);  //TODO: check: if this is the src node, use dsr->src instead

  BRN_DEBUG("Use Source: %s",src.unparse().c_str());

  BRNPacketAnno::set_src_ether_anno(p_out,src);  //TODO: CHECK
  if (index == brn_dsr->dsr_hop_count) {// no hops left; use final dst
    BRN_DEBUG(" * using final dst. %d %d", brn_dsr->dsr_hop_count, index);
    BRNPacketAnno::set_dst_ether_anno(p_out,EtherAddress(brn_dsr->dsr_dst.data));
  } else {
    BRNPacketAnno::set_dst_ether_anno(p_out,EtherAddress(dsr_hops[index].hw.data));
  }
  BRNPacketAnno::set_ethertype_anno(p_out,ETHERTYPE_BRN);

  return p_out;
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

    const click_brn_dsr *brn_dsr = reinterpret_cast<const click_brn_dsr *>((p_in->data() + sizeof(click_brn)));

    assert(brn_dsr->dsr_type == BRN_DSR_SRC);

    // fetch the final destination of dsr source packet
    EtherAddress dst_addr(brn_dsr->dsr_dst.data);

    // learn from this packet
    BRN2RouteQuerierRoute source_route;
    _dsr_decap->extract_source_route(p_in, source_route);

    if (_debug == BrnLogger::DEBUG) {
      BRN_DEBUG(" * learned from SRCPacket ...");
      for (int j = 0; j < source_route.size(); j++)
        BRN_DEBUG(" SRC - %d   %s (%d)",
          j, source_route[j].ether().unparse().c_str(), source_route[j]._metric);
    }

    struct click_brn *brnh = (struct click_brn *)p_in->data();
    brnh->ttl--;
    BRNPacketAnno::set_ttl_anno(p_in, brnh->ttl);
    int source_hops  = brn_dsr->dsr_hop_count;
    int segments     = brn_dsr->dsr_segsleft;
    int index = source_hops - segments - 2;

    // update link table
    _route_querier->add_route_to_link_table(source_route, DSR_ELEMENT_SRC_FORWARDER, index);

    //BRN_DEBUG(_link_table->print_links().c_str());

    BRN_DEBUG("Forward ?? DEST: %s ME: %s", dst_addr.unparse().c_str(),
                                            _me->getMasterAddress()->unparse().c_str());

    if (_me->isIdentical(&dst_addr) || ( _link_table->is_associated(dst_addr))) { // for me
      BRN_DEBUG(" * source routed packet reached final destination (me or my assoc clients)");
      BRN_DEBUG("Final Dest: %s",dst_addr.unparse().c_str());
      if ( _dsr_rid_cache ) {

        EtherAddress dst(brn_dsr->dsr_dst.data);
        EtherAddress next(brn_dsr->dsr_dst.data);
        EtherAddress src(brn_dsr->dsr_src.data);

        const click_dsr_hop *dsr_hops = DSRProtocol::get_hops(brn_dsr);
        EtherAddress last(dsr_hops[brn_dsr->dsr_hop_count - 1].hw.data);

        BrnRouteIdCache::RouteIdEntry* rid_e = _dsr_rid_cache->get_entry(&src, ntohs(brn_dsr->dsr_id));
        if ( ( ! rid_e ) && (ntohs(brn_dsr->dsr_id) != 0) ) {
          rid_e = _dsr_rid_cache->insert_entry(&src, &dst, &last, &next, ntohs(brn_dsr->dsr_id));
          if ( rid_e == NULL ) {
            BRN_ERROR("DSR-ID-Cache error");
          }
        }
      }
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

  WritablePacket *p_out = _dsr_encap->set_packet_to_next_hop(p_in);

  // check the link metric from me to the dst
  click_brn_dsr *brn_dsr = reinterpret_cast<click_brn_dsr *>((p_out->data() + sizeof(click_brn)));

  int source_hops  = brn_dsr->dsr_hop_count;
  int segments     = brn_dsr->dsr_segsleft;

  // nothing todo for source_hops == 0 (src, dst)
  assert(source_hops > 0);
  assert(segments >= 0);

  int index = source_hops - segments;

  EtherAddress me(brn_dsr->dsr_src.data);
  EtherAddress next(brn_dsr->dsr_dst.data);

  EtherAddress dst(brn_dsr->dsr_dst.data);
  EtherAddress src(brn_dsr->dsr_src.data);
  EtherAddress last(brn_dsr->dsr_src.data);

  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(brn_dsr);//RobAt:DSR

  if ( segments + 1 < source_hops )
    last = EtherAddress (dsr_hops[index - 2].hw.data);

  if (segments < source_hops) {
    // intermediate hop
    me = EtherAddress (dsr_hops[index - 1].hw.data);
    dsr_hops[index - 1].metric = htons(_link_table->get_link_metric(last, me));//update metric in route from last to me 26.11.2010
  } else {
    // first hop
  }

  if (segments > 0)     // NOT the last hop (if it is the last hop, then next is dst)
    next = EtherAddress (dsr_hops[index].hw.data);

  if ( _dsr_rid_cache ) {
    BrnRouteIdCache::RouteIdEntry* rid_e = _dsr_rid_cache->get_entry(&src, ntohs(brn_dsr->dsr_id));
    if ( rid_e ) {
      next = EtherAddress(rid_e->_next_hop.data());
      rid_e->update();
    } else {
      if ( ntohs(brn_dsr->dsr_id) != 0 ) {
        rid_e = _dsr_rid_cache->insert_entry(&src, &dst, &last, &next, ntohs(brn_dsr->dsr_id));
        if ( rid_e == NULL ) {
          BRN_ERROR("DSR-ID-Cache error");
        }
      }
    }
  }

  WritablePacket* p = p_in->uniqueify();
  // Check if the next hop exists in link table    
  if (BRN_DSR_INVALID_ROUTE_METRIC == _link_table->get_link_metric(me, next)) {
    BRN_DEBUG(" * BRN2SrcForwarder: no link between %s and %s, broken source route.", me.unparse().c_str(), next.unparse().c_str());
    BRN_DEBUG(_link_table->print_links().c_str());

    click_ether *ether = reinterpret_cast<click_ether *>(p->ether_header());

    memcpy(ether->ether_shost, me.data(), sizeof(ether->ether_shost));
    memcpy(ether->ether_dhost, next.data(), sizeof(ether->ether_dhost));
    ether->ether_type = htons(ETHERTYPE_BRN);                             //TODO: CHECK: this is important ??

    output(2).push(p);  //Route-Error
    return;
  }

  click_ether *ether = reinterpret_cast<click_ether *>(p->ether_header());//TODO: CHECK: this is important ??
  memcpy(ether->ether_shost, me.data(), sizeof(ether->ether_shost));  //TODO: copx
  memcpy(ether->ether_dhost, next.data(), sizeof(ether->ether_dhost));  //TODO: is a copy
  ether->ether_type = htons(ETHERTYPE_BRN);                //TODO: CHECK: this is important ??
  // ouput packet
  BRNPacketAnno::set_src_ether_anno(p_out,me);                        //TODO: CHECK
  BRNPacketAnno::set_dst_ether_anno(p_out,next);
  BRNPacketAnno::set_ethertype_anno(p_out,ETHERTYPE_BRN);

  output(0).push(p_out);
}

Packet *
BRN2SrcForwarder::strip_all_headers(Packet *p_in)
{
  const click_brn_dsr *dsr_src = reinterpret_cast<const click_brn_dsr *>((p_in->data() + sizeof(click_brn)));

  int brn_dsr_len = sizeof(click_brn) + DSRProtocol::header_length(dsr_src);

  EtherAddress src(dsr_src->dsr_src.data);
  EtherAddress dst(dsr_src->dsr_dst.data);

  WritablePacket *p = p_in->uniqueify();

  // remove the headers  
  p->pull(brn_dsr_len);

  // ether anno
  click_ether* ether_anno = const_cast<click_ether*>(p->ether_header());
  if( NULL == ether_anno ) {
    p = p->push_mac_header(sizeof(click_ether));
    //FIXME: ether_anno is never used
    ether_anno = const_cast<click_ether*>(p->ether_header());

    //TODO: ether type cannot be set here.
    p->pull(sizeof(click_ether));
    //BRN_DEBUG("PUSH Etherheader");
  }

  ether_anno = reinterpret_cast<click_ether*>(p->data());
  p->set_ether_header(ether_anno);

  memcpy( ether_anno->ether_shost, src.data(), 6 );
  memcpy( ether_anno->ether_dhost, dst.data(), 6 );

  //BRN_DEBUG(" * stripping headers; removed %d bytes", brn_dsr_len);
  //BRN_DEBUG("SRC: %s DST: %s",src.unparse().c_str(),dst.unparse().c_str());

  if ( BRNProtocol::is_brn_etherframe(p) ) { //set ttl if payload is brn_packet
    struct click_brn *brnh = BRNProtocol::get_brnheader_in_etherframe(p);
    brnh->ttl = BRNPacketAnno::ttl_anno(p);
  }

  return p;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BRN2SrcForwarder::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2SrcForwarder)
