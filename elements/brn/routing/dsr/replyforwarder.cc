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

#include "replyforwarder.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

CLICK_DECLS

ReplyForwarder::ReplyForwarder()
  : _debug(BrnLogger::DEFAULT),
    _me(),
    _dsr_encap(),
    _dsr_decap(),
    _route_querier(),
    _client_assoc_lst(),
    _link_table()
{
  //add_input(); // previously generated reply packet needs to be forwarded
  //add_input(); // receiving reply packet (dest reached or packet has to be forwarded)

  //add_output(); //rrep has to be forwarded
}

ReplyForwarder::~ReplyForwarder()
{
}

int
ReplyForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      cpOptional,
      cpElement, "NodeIdentity", &_me,
      cpElement, "DSRDecap", &_dsr_decap,
      cpElement, "RouteQuerier", &_route_querier,
      cpElement, "Client assoc list", &_client_assoc_lst,
      cpElement, "DSREncap", &_dsr_encap,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_dsr_decap || !_dsr_decap->cast("DSRDecap")) 
    return errh->error("DSRDecap not specified");

  if (!_route_querier || !_route_querier->cast("RouteQuerier")) 
    return errh->error("RouteQuerier not specified");

  if (!_client_assoc_lst || !_client_assoc_lst->cast("AssocList")) 
    return errh->error("AssocList not specified");

  if (!_dsr_encap || !_dsr_encap->cast("DSREncap")) 
    return errh->error("DSREncap not specified");

  return 0;
}

int
ReplyForwarder::initialize(ErrorHandler *errh)
{
  _link_table = _me->get_link_table();
  if (!_link_table || !_link_table->cast("BrnLinkTable")) 
    return errh->error("BRNLinkTable not specified");

  return 0;
}

void
ReplyForwarder::uninitialize()
{
  //cleanup
}

/*
 * Process an incoming route request.  If it's for us, issue a reply.
 * If not, check for an entry in the request table, and insert one and
 * forward the request if there is not one.
 */
void
ReplyForwarder::push(int port, Packet *p_in)
{
  BRN_DEBUG("push()");

  if (port == 0) { //previously created rrep packets
    BRN_DEBUG(" * receiving dsr_rrep packet; port 0");

    Packet *p_out = _me->skipInMemoryHops(p_in);
    // packet has to be forwarded
    output(0).push(p_out);

  } else if (port == 1) { // rrep packet received by this node

    const click_brn_dsr *brn_dsr =
          (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

    BRN_DEBUG(" * receiving dsr_rrep packet; port 1; #ID %d", ntohs(brn_dsr->body.rreq.dsr_id));

    assert(brn_dsr->dsr_type == BRN_DSR_RREP);

    //destination of route reply packet
    EtherAddress dst_addr(brn_dsr->dsr_dst.data);

    // extract the reply route..  convert to node IDs and add to the
    // link cache
    RouteQuerierRoute reply_route;

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

    if (_me->isIdentical(&dst_addr) || _client_assoc_lst->is_associated(dst_addr)) {
      // the first address listed in the route reply's route must be
      // the destination which we queried; this is not necessarily
      // the same as the destination in the IP header because we
      // might be doing reply-from-cache  
      EtherAddress reply_dst = EtherAddress(brn_dsr->dsr_src.data);
      BRN_DEBUG(" * killed (route to %s reached final destination, %s, #ID %d)",
          reply_dst.unparse().c_str(), dst_addr.unparse().c_str(), ntohs(brn_dsr->body.rreq.dsr_id));
      _route_querier->stop_issuing_request(reply_dst);
      p_in->kill();
      return;
    } else {
      BRN_DEBUG(" * forwarding RREP towards destination %s, #ID %d",
        dst_addr.unparse().c_str(), ntohs(brn_dsr->body.rreq.dsr_id));
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
ReplyForwarder::forward_rrep(Packet * p_in)
{
  BRN_DEBUG(" * forward_rrep: ...");

  Packet *p_out = _dsr_encap->set_packet_to_next_hop(p_in);
  //p_out = _req_forwarder->_dsr_encap->set_packet_to_next_hop(p_in);
  // ouput packet
  output(0).push(p_out);
}

void
ReplyForwarder::add_route_to_link_table(const RouteQuerierRoute &route)
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
  ReplyForwarder *rf = (ReplyForwarder *)e;
  return String(rf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  ReplyForwarder *rf = (ReplyForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  rf->_debug = debug;
  return 0;
}

void
ReplyForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ReplyForwarder)
