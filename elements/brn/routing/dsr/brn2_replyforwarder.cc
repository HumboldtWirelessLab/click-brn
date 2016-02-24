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
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "brn2_dsrprotocol.hh"
#include "brn2_replyforwarder.hh"


CLICK_DECLS

BRN2ReplyForwarder::BRN2ReplyForwarder()
  : _me(),
    _dsr_encap(),
    _dsr_decap(),
    _route_querier(),
    _link_table()
{
  BRNElement::init();
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
      "DEBUG", cpkP, cpInteger, &_debug,
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

    Packet *p_out = _dsr_encap->skipInMemoryHops(p_in);

    BRN_DEBUG(" * forward_rrep: Next Hop is %s",EtherAddress(BRNPacketAnno::dst_ether_anno(p_out)).unparse().c_str());

    // packet has to be forwarded
    output(0).push(p_out);

  } else if (port == 1) { // rrep packet received by this node

    const click_brn_dsr *brn_dsr = reinterpret_cast<const click_brn_dsr *>(p_in->data() + sizeof(click_brn));

    BRN_DEBUG(" * receiving dsr_rrep packet; port 1; #ID %d", ntohs(brn_dsr->dsr_id));

    assert(brn_dsr->dsr_type == BRN_DSR_RREP);

    //destination of route reply packet
    EtherAddress dst_addr(brn_dsr->dsr_dst.data);

    // extract the reply route..  convert to node IDs and add to the
    // link cache
    BRN2RouteQuerierRoute reply_route;

    _dsr_decap->extract_reply_route(p_in, reply_route);

    if ( _debug == BrnLogger::DEBUG ) {
      uint32_t this_metric = _route_querier->route_metric(reply_route);

      BRN_DEBUG(" * learned from RREP ... route: size = %d metric = %d", reply_route.size(), this_metric);
    }

    for (int j = 0; j < reply_route.size(); j++) {
      BRN_DEBUG(" RREP - %d   %s (%d)",
                  j, reply_route[j].ether().unparse().c_str(),
                  reply_route[j]._metric);
    }

    // XXX really, is this necessary?  or are we only potentially
    // making the link data more stale, while marking it as current?
    _route_querier->add_route_to_link_table(reply_route, DSR_ELEMENT_REP_FORWARDER, -1);

    // now check for packets in the sendbuffer whose destination has
    // been found using the information from the route reply
    _route_querier->_sendbuffer_check_routes = true;
    _route_querier->_sendbuffer_timer.schedule_now();

    // remove the last forwarder from the blacklist, if present
    const click_ether * ether = reinterpret_cast<const click_ether *>(p_in->ether_header());
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

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BRN2ReplyForwarder::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2ReplyForwarder)
