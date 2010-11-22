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
 * replyforwarder.{cc,hh} -- forwards dsr route reply packets
 * A. Zubow
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "brn2_dsrprotocol.hh"
#include "brn2_replyforwarder.hh"


CLICK_DECLS

BRN2ReplyForwarder::BRN2ReplyForwarder()
  : _debug(BrnLogger::DEFAULT),
    _me(),
    _dsr_encap(),
    _dsr_decap(),
    _route_querier(),
    _link_table()
{
  //add_input(); // previously generated reply packet needs to be forwarded
  //add_input(); // receiving reply packet (dest reached or packet has to be forwarded)

  //add_output(); //rrep has to be forwarded
}

BRN2ReplyForwarder::~BRN2ReplyForwarder()
{
}

int
BRN2ReplyForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      "DSRDECAP", cpkP+cpkM, cpElement, &_dsr_decap,
      "ROUTEQUERIER", cpkP+cpkM, cpElement, &_route_querier,
      "DSRENCAP", cpkP+cpkM, cpElement, &_dsr_encap,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_dsr_decap || !_dsr_decap->cast("BRN2DSRDecap")) 
    return errh->error("DSRDecap not specified");

  if (!_route_querier || !_route_querier->cast("BRN2RouteQuerier")) 
    return errh->error("RouteQuerier not specified");

  if (!_dsr_encap || !_dsr_encap->cast("BRN2DSREncap"))
    return errh->error("DSREncap not specified");

  return 0;
}

int
BRN2ReplyForwarder::initialize(ErrorHandler *)
{
  return 0;
}

void
BRN2ReplyForwarder::uninitialize()
{
  //cleanup
}

Packet *
BRN2ReplyForwarder::skipInMemoryHops(Packet *p_in)
{
  BRN_DEBUG(" * calling BRN2ReplyForwarder::skipInMemoryHops().");

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

/*
 * Process an incoming route request.  If it's for us, issue a reply.
 * If not, check for an entry in the request table, and insert one and
 * forward the request if there is not one.
 */
void
BRN2ReplyForwarder::push(int port, Packet *p_in)
{
  BRN_DEBUG("push()");

  if (port == 0) { //previously created rrep packets
    BRN_DEBUG(" * receiving dsr_rrep packet; port 0");

    Packet *p_out = skipInMemoryHops(p_in);

    BRN_DEBUG(" * forward_rrep: Next Hop is %s",EtherAddress(BRNPacketAnno::dst_ether_anno(p_out)).unparse().c_str());

    // packet has to be forwarded
    output(0).push(p_out);

  } else if (port == 1) { // rrep packet received by this node

    const click_brn_dsr *brn_dsr =
          (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

    BRN_DEBUG(" * receiving dsr_rrep packet; port 1; #ID %d", ntohs(brn_dsr->dsr_id));

    assert(brn_dsr->dsr_type == BRN_DSR_RREP);

    //destination of route reply packet
    EtherAddress dst_addr(brn_dsr->dsr_dst.data);

    // extract the reply route..  convert to node IDs and add to the
    // link cache
    BRN2RouteQuerierRoute reply_route;

    _dsr_decap->extract_reply_route(p_in, reply_route);

    BRN_DEBUG(" * learned from RREP ... route: size = %d", reply_route.size());

    for (int j = 0; j < reply_route.size(); j++) {
      BRN_DEBUG(" RREP - %d   %s (%d)",
                  j, reply_route[j].ether().unparse().c_str(),
                  reply_route[j]._metric);
    }

    // XXX really, is this necessary?  or are we only potentially
    // making the link data more stale, while marking it as current?
    add_route_to_link_table(reply_route);

    // now check for packets in the sendbuffer whose destination has
    // been found using the information from the route reply
    _route_querier->_sendbuffer_check_routes = true;
    _route_querier->_sendbuffer_timer.schedule_now();

    // remove the last forwarder from the blacklist, if present
    const click_ether * ether = (const click_ether *)p_in->ether_header();
    EtherAddress last_forwarder(ether->ether_shost);

    BRN_DEBUG(" last_forwarder is %s", last_forwarder.unparse().c_str());

    _route_querier->set_blacklist(last_forwarder, BRN_DSR_BLACKLIST_NOENTRY);

    if (_me->isIdentical(&dst_addr) || (_link_table->is_associated(dst_addr))) {
      // the first address listed in the route reply's route must be
      // the destination which we queried; this is not necessarily
      // the same as the destination in the IP header because we
      // might be doing reply-from-cache  
      EtherAddress reply_dst = EtherAddress(brn_dsr->dsr_src.data);
      BRN_DEBUG(" * killed (route to %s reached final destination, %s, #ID %d)",
          reply_dst.unparse().c_str(), dst_addr.unparse().c_str(), ntohs(brn_dsr->dsr_id));
      _route_querier->stop_issuing_request(reply_dst);
      p_in->kill();
      return;
    } else {
      BRN_DEBUG(" * forwarding RREP towards destination %s, #ID %d",
        dst_addr.unparse().c_str(), ntohs(brn_dsr->dsr_id));
      forward_rrep(p_in); // determines next hop, sets dest ip anno, and then pushes out to arp table.
      return;
    }
  } else {
    BRN_ERROR(" * unknow port !!!!!!!!");
    p_in->kill();
  }
}

/*
 * p_in is a route reply which we received because we're on its source
 * route; return the packet.
 */
void
BRN2ReplyForwarder::forward_rrep(Packet * p_in)
{
  BRN_DEBUG(" * forward_rrep: ...");

  //next hop (ether anno) is set by set_packet_to_next_hop
  Packet *p_out = _dsr_encap->set_packet_to_next_hop(p_in);

  BRN_DEBUG(" * forward_rrep: Next Hop is %s",EtherAddress(BRNPacketAnno::dst_ether_anno(p_in)).unparse().c_str());

  // ouput packet
  output(0).push(p_out);
}

void
BRN2ReplyForwarder::add_route_to_link_table(const BRN2RouteQuerierRoute &route)
{
  for (int i = 0; i < route.size() - 1; i++) {
    EtherAddress ether1 = route[i].ether();
    EtherAddress ether2 = route[i+1].ether();

    if (ether1 == ether2)
      continue;

    if (_me->isIdentical(&ether1)) // learn only from route prefix; suffix is not yet set
      break;

    uint16_t metric = route[i+1]._metric; //metric starts with offset 1

    //TODO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! remove this shit
    if (metric == 0)
      metric = 1;
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

    if (ret)
      BRN_DEBUG(" _link_table->update_link %s (%s) %s (%s) %d\n",
        route[i].ether().unparse().c_str(), route[i].ip().unparse().c_str(),
        route[i+1].ether().unparse().c_str(), route[i+1].ip().unparse().c_str(), metric);
  }

  BRN_DEBUG(" * My Linktable: \n%s", _link_table->print_links().c_str());
}
//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRN2ReplyForwarder *rf = (BRN2ReplyForwarder *)e;
  return String(rf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRN2ReplyForwarder *rf = (BRN2ReplyForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  rf->_debug = debug;
  return 0;
}

void
BRN2ReplyForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2ReplyForwarder)
