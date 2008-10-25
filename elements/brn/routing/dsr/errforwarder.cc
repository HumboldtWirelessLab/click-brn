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
 * errforwarder.{cc,hh} -- forwards dsr route error packets
 * A. Zubow
 */

#include <click/config.h>

#include "errforwarder.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

CLICK_DECLS

ErrorForwarder::ErrorForwarder()
  : _debug(BrnLogger::DEFAULT),
  _me(),
  _link_table(),
  _client_assoc_lst(),
  _dsr_encap(),
  _dsr_decap(),
  _route_querier(),
  _brn_encap()
{
  //add_input();  //process previously by the ds reported link level error
  //add_input();  //process incoming dsr rerr packet
  //add_output(); //forward dsr rerr packet to final destination
}

ErrorForwarder::~ErrorForwarder()
{
}

int
ErrorForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, /*"NodeIdentity",*/ &_me,
      "ASSOCLIST", cpkP+cpkM, cpElement, /*"Client assoc list",*/ &_client_assoc_lst,
      "DSRENCAP", cpkP+cpkM, cpElement, /*"DSREncap",*/ &_dsr_encap,
      "DSRDECAP", cpkP+cpkM, cpElement, /*"DSRDecap",*/ &_dsr_decap,
      "ROUTEQUERIER", cpkP+cpkM, cpElement, /*"RouteQuerier",*/ &_route_querier,
      "BRNENCAP", cpkP+cpkM, cpElement, /*"BRNEncap",*/ &_brn_encap,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_client_assoc_lst || !_client_assoc_lst->cast("AssocList")) 
    return errh->error("ClientAssocList not specified");

  if (!_dsr_encap || !_dsr_encap->cast("DSREncap")) 
    return errh->error("DSREncap not specified");

  if (!_dsr_decap || !_dsr_decap->cast("DSRDecap")) 
    return errh->error("DSRDecap not specified");

  if (!_route_querier || !_route_querier->cast("RouteQuerier")) 
    return errh->error("RouteQuerier not specified");

  if (!_brn_encap || !_brn_encap->cast("BRNEncap")) 
    return errh->error("BRNEncap not specified");

  return 0;
}

int
ErrorForwarder::initialize(ErrorHandler *errh)
{
  _link_table = _me->get_link_table();
  if (!_link_table || !_link_table->cast("BrnLinkTable")) 
    return errh->error("BRNLinkTable not specified");

  return 0;
}

void
ErrorForwarder::uninitialize()
{
  //cleanup
}

void
ErrorForwarder::push(int port, Packet *p_in)
{
  BRN_DEBUG("* push()");

  if (port == 0) { //process previously by the ds reported link level error

    BRN_DEBUG("* handle link level errors...");

    // source-routed packet whose transmission to the next hop failed
    // fetch some header
    const click_ether *ether = (const click_ether *)p_in->ether_header();
    const click_brn_dsr *dsr = (const click_brn_dsr *)(p_in->data() + sizeof(click_brn));

    EtherAddress bad_src(ether->ether_shost); // ether address of current node
    EtherAddress bad_dst(ether->ether_dhost);

    BRN_DEBUG("* packet had bad source route with next hop %s and curr hop %s",
        bad_dst.unparse().c_str(), bad_src.unparse().c_str());

    if (dsr->dsr_type == BRN_DSR_RREP) {
      BRN_DEBUG("* tx error sending route reply; adding entry to blacklist for %s",
          bad_dst.unparse().c_str());
      _route_querier->set_blacklist(bad_dst, BRN_DSR_BLACKLIST_UNI_PROBABLE);

    } else if (dsr->dsr_type == BRN_DSR_RREQ) {
      // XXX are we only supposed to set this for failed RREPs?
      BRN_DEBUG("* one-hop unicast RREQ failed.");
      _route_querier->set_blacklist(bad_dst, BRN_DSR_BLACKLIST_UNI_PROBABLE);
    }

    BRN_DEBUG("- removing link from %s to %s", bad_src.unparse().c_str(), bad_dst.unparse().c_str());

    // remove all links between nodes bad_src and bad_dst
    _link_table->update_both_links(bad_src, bad_dst, 0, 0, BRN_DSR_INVALID_ROUTE_METRIC);

    //TODO: Azu Is this really a good idea???
    // it is possible that the node is no longer available
    // punish all links that are dealing with this node
    Vector<EtherAddress> neighbors;

    _link_table->get_neighbors(bad_dst, neighbors);

    int i;
    for ( i = 0; i < neighbors.size(); i++ ) 
    {
      // punish all links of bad_dst
      uint32_t old_metric = _link_table->get_link_metric(neighbors[i], bad_dst);

      old_metric *= BRN_DSR_ROUTE_ERR_NEIGHBORS_PUNISHMENT_FAC;
      if (old_metric > BRN_DSR_INVALID_ROUTE_METRIC)
        old_metric = BRN_DSR_INVALID_ROUTE_METRIC;

      BRN_DEBUG("- punish link from %s to %s", 
          neighbors[i].unparse().c_str(), bad_dst.unparse().c_str());

      _link_table->update_both_links(neighbors[i], bad_dst, 0, 0, old_metric);
    }

    BRN_DEBUG(" * current links: %s", _link_table->print_links().c_str());

    // need to send a route error
    RouteQuerierRoute source_route, trunc_route, rev_route;

    // send RERR back along its original source route
    _dsr_decap->extract_source_route(p_in, source_route);

    truncate_route(source_route, bad_src, trunc_route);

    if(_debug)
      for (int j = 0; j < trunc_route.size(); j++) {
        BRN_DEBUG(" trunc_route - %d  %s", j, trunc_route[j].ether_address.unparse().c_str());
      }

    if (! trunc_route.size()) {
      // this would suggest something is very broken
      BRN_DEBUG("* couldn't find my address in bad source route!");
      return;
    }

    reverse_route(trunc_route, rev_route);
    EtherAddress src_addr(dsr->dsr_src.data); // originator of this msg
    if (!_client_assoc_lst->is_associated(src_addr) && !_me->isIdentical(&src_addr)) {
      issue_rerr(bad_src, bad_dst, src_addr, rev_route);
    } else {
      BRN_DEBUG("* originator is associated with me; no rerr!");
    }

    // find packets with this link in their source route and yank
    // them out of our outgoing queue
/*
    if (_outq) {
      Vector<Packet *> y;
      _outq->yank(link_filter(bad_src, bad_dst), y);
      click_chatter(" * yanked %d packets; killing...\n", y.size());
      for (int i = 0; i < y.size(); i++)
        y[i]->kill();
    }
*/
    //   // salvage the packet?
    //   if (dsr_option->dsr_type == DSR_TYPE_RREP) {
    //   	// we don't salvage replies
    //   	p_in->kill();
    //   	return;
    //   } else if (dsr_option->dsr_type == DSR_TYPE_RREQ) {
    //   	// unicast route request must be from me... this case should
    //   	// never happen.
    //   	assert(0);
    //   	return;
    //   } else if (dsr_option->dsr_type == DSR_TYPE_RERR) {
    //   	// ah, i don't know.  this is complicated.  XXX
    //   } else if (dsr_option->dsr_type == DSR_TYPE_SOURCE_ROUTE) {
    //   	salvage(p_in);
    //   	return;
    //   }

    p_in->kill();
    return;
  } else if (port == 1) { //process incoming dsr rerr packet

    BRN_DEBUG(" * receiving dsr rerr");

    const click_brn_dsr *brn_dsr =
          (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

    // only handled type right now
    assert(brn_dsr->body.rerr.dsr_error == BRN_DSR_RERR_TYPE_NODE_UNREACHABLE);

    // get the bad hops
    EtherAddress err_src(brn_dsr->body.rerr.dsr_unreachable_src.data);
    EtherAddress err_dst(brn_dsr->dsr_dst.data); // the originator of the failed data packet
    EtherAddress unreachable(brn_dsr->body.rerr.dsr_unreachable_dst.data);

    // now remove the entries from the linkcache
    BRN_DEBUG(" - removing link from %s to %s; rerr destination is %s",
        err_src.unparse().c_str(), unreachable.unparse().c_str(), err_dst.unparse().c_str());

    // XXX DSR_INVALID_HOP_METRIC isn't really an appropriate name here
    _link_table->update_both_links(err_src, unreachable, 0, 0, BRN_DSR_INVALID_ROUTE_METRIC);

    if (_me->isIdentical(&err_dst) || _client_assoc_lst->is_associated(err_dst)) {
      BRN_DEBUG("* error packet killed (reached final destination)");
      p_in->kill();
    } else {
      forward_rerr(p_in);
    }

    // find packets with this link in their source route and yank
    // them out of our outgoing queue
  //TODO think about this!!!!!!!!!!
  /*
    if (_outq) {
      Vector<Packet *> y;
      _outq->yank(link_filter(err_src, unreachable), y);
      click_chatter("yanked %d packets; killing...\n", y.size());
      for (int i = 0; i < y.size(); i++)
        y[i]->kill();
    }
  */
  } else {
    BRN_ERROR(" * unknown port; drop packet.");
    p_in->kill();
  }
}

void
ErrorForwarder::forward_rerr(Packet *p_in)
{
  BRN_DEBUG("* forward_rerr: ...");

  Packet *p_out = _dsr_encap->set_packet_to_next_hop(p_in);
  // ouput packet
  output(0).push(p_out);
}

/* method generates and sends a dsr route reply */
void
ErrorForwarder::issue_rerr(EtherAddress bad_src, EtherAddress bad_dst,
                          EtherAddress src, const RouteQuerierRoute &source_route)
{
  BRN_DEBUG("* issue_rrer: ...");

  Packet *brn_p;

  //prepend dsr header
  Packet *rrer_p = _dsr_encap->create_rerr(bad_src, bad_dst, src, source_route);
  //prepend brn header
  assert(rrer_p);
  brn_p = _brn_encap->add_brn_header(rrer_p);

  //skip inMemory hops
  brn_p = _me->skipInMemoryHops(brn_p);

  //output route error packet
  output(0).push(brn_p);
}


void
ErrorForwarder::truncate_route(const RouteQuerierRoute &r, EtherAddress bad_src, RouteQuerierRoute &t)
{
 for (int i=0; i < r.size(); i++) {
   t.push_back(r[i]);
   if (r[i].ether() == bad_src) {
     return;
   }
 }
 return;
}

void
ErrorForwarder::reverse_route(const RouteQuerierRoute &r, RouteQuerierRoute &rev)
{
  for(int i=r.size()-1; i>=0; i--) {
    rev.push_back(r[i]);
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  ErrorForwarder *ef = (ErrorForwarder *)e;
  return String(ef->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  ErrorForwarder *ef = (ErrorForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ef->_debug = debug;
  return 0;
}

void
ErrorForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(ErrorForwarder)
