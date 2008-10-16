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
 * routequerier.{cc,hh} - searches for a valid route for a given packet
 *
 * Zubow A. 
 * thanks to Douglas S. J. De Couto
 */
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include "routequerier.hh"
#include "elements/brn/routing/metric/genericmetric.hh"
#include "elements/brn/wifi/ap/iapp/brniappstationtracker.hh"
//#include "printetheranno.hh"

CLICK_DECLS

/* constructor initalizes timer, ... */
RouteQuerier::RouteQuerier()
  : _debug(BrnLogger::DEFAULT),
    _sendbuffer_check_routes(false),
    _sendbuffer_timer(static_sendbuffer_timer_hook, this),
    _me(NULL),
    _link_table(),
    _dsr_encap(),
    _brn_encap(),
    _dsr_decap(),
    _brn_iapp(NULL),
    _rreq_expire_timer(static_rreq_expire_hook, this),
    _rreq_issue_timer(static_rreq_issue_hook, this),
    _blacklist_timer(static_blacklist_timer_hook, this),
    _metric(0),
    _use_blacklist(true)
{
  timeval tv;
  click_gettimeofday(&tv);
  _rreq_id = random() % 0xffff;

  //add_input();  // incoming packets

  //add_output(); // output 0: route to destination is not available (rreq packet)
  //add_output(); // output 1: route to destination is available (move packet to srcforwarder)
}

/* destructor processes some cleanup */
RouteQuerier::~RouteQuerier()
{
  flush_sendbuffer();
  uninitialize();

  for (FWReqIter i = _forwarded_rreq_map.begin(); i.live(); i++) {
    ForwardedReqVal &frv = i.value();
    if (frv.p) frv.p->kill();
    frv.p = NULL;
  }
}

/* called by click at configuration time */
int
RouteQuerier::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
      cpOptional,
      cpElement, "NodeIdentity", &_me,
      cpElement, "DSREncap", &_dsr_encap,
      cpElement, "BRNEncap", &_brn_encap,
      cpElement, "DSRDecap", &_dsr_decap,
      cpElement, "BrnIappStationTracker", &_brn_iapp,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_dsr_encap || !_dsr_encap->cast("DSREncap")) 
    return errh->error("DSREncap not specified");

  if (!_brn_encap || !_brn_encap->cast("BRNEncap")) 
    return errh->error("BRNEncap not specified");

  if (!_dsr_decap || !_dsr_decap->cast("DSRDecap")) 
    return errh->error("DSRDecap not specified");

  if (!_brn_iapp || !_brn_iapp->cast("BrnIappStationTracker")) 
    return errh->error("BrnIappStationTracker not specified");

  return 0;
}

/* initializes error handler */
int
RouteQuerier::initialize(ErrorHandler *errh)
{
  _link_table = _me->get_link_table();
  if (!_link_table || !_link_table->cast("BrnLinkTable")) 
    return errh->error("BRNLinkTable not specified");

  // expire entries on list of rreq's we have seen
  _rreq_expire_timer.initialize(this);
  _rreq_expire_timer.schedule_after_msec(BRN_DSR_RREQ_EXPIRE_TIMER_INTERVAL);

  // expire packets in the sendbuffer
  _sendbuffer_timer.initialize(this);
  _sendbuffer_timer.schedule_after_msec(BRN_DSR_SENDBUFFER_TIMER_INTERVAL);

  // check if it's time to reissue a route request
  _rreq_issue_timer.initialize(this);
  _rreq_issue_timer.schedule_after_msec(BRN_DSR_RREQ_ISSUE_TIMER_INTERVAL);

  _blacklist_timer.initialize(this);
  _blacklist_timer.schedule_after_msec(BRN_DSR_BLACKLIST_TIMER_INTERVAL);

  return 0;
}

/* uninitializes error handler */
void
RouteQuerier::uninitialize()
{
  _rreq_expire_timer.unschedule();
  _rreq_issue_timer.unschedule();
  _sendbuffer_timer.unschedule();
  _blacklist_timer.unschedule();
}

/*
 * This method queries the link table for a route to the given destination.
 * If no route is available the packet is buffered a route request is generated.
 */
void
RouteQuerier::push(int, Packet *p_in)
{
  click_ether *ether = (click_ether *)p_in->ether_header();
  EtherAddresses route;

  if (!ether) {
    BRN_DEBUG(" * ether anno not available; drop packet.");
    p_in->kill();
    return;
  }

  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  if (!dst_addr || !src_addr) {
    BRN_ERROR(" ethernet anno header is null; kill packet.");
    //PrintEtherAnno::print("bogus packet.", p_in);
    p_in->kill();
    return;
  }

  int metric_of_route = -1;
  EtherAddresses* fixed_route = fixed_routes.findp(EtherPair(dst_addr, src_addr));
  if ( fixed_route && (fixed_route->size() >= 2) 
    && ((*fixed_route)[0] == dst_addr) 
    && ((*fixed_route)[fixed_route->size() - 1] == src_addr) ) {
      BRN_DEBUG("* Using fixed route. %s, (%s), %s, (%s)", 
        (*fixed_route)[0].s().c_str(), dst_addr.s().c_str(),
        (*fixed_route)[fixed_route->size() - 1].s().c_str(), src_addr.s().c_str());
    route = *fixed_route;
    metric_of_route = 1;
  } else {
    BRN_DEBUG(" query route");
    _link_table->query_route( src_addr, dst_addr, route );
    metric_of_route = _link_table->get_route_metric(route);
  }

  BRN_DEBUG(" Metric of found route = %d", metric_of_route); 

  if(_debug == BrnLogger::DEBUG) {
    String route_str = _link_table->print_routes(true);
    BRN_DEBUG(" * routestr: %s", route_str.c_str());
  }

  if ( ( route.size() > 1 ) && ( metric_of_route != -1 ) ) { // route available
    if(_debug == BrnLogger::DEBUG) {
      BRN_DEBUG(" * have cached route:");
      for (int j=0; j < route.size(); j++) {
        BRN_DEBUG(" - %d  %s", j, route[j].s().c_str());
      }
    }

    // add DSR headers to packet..
    Packet *dsr_p = _dsr_encap->add_src_header(p_in, route); //todo brn_dsr packet --> encap
    // add BRN header
    Packet *brn_p = _brn_encap->add_brn_header(dsr_p);

    // forward source routed packet to srcforwarder. this is required since
    // the address of the next hop could be mine (see nodeidentity)
    output(1).push(brn_p);
    return;

  } else {

    if (_debug == BrnLogger::DEBUG) {
      if (metric_of_route == -1) {
        BRN_DEBUG(" * metric of route is too bad ! drop it !");
        BRN_DEBUG(" * My Linktable: \n%s", _link_table->print_links().c_str());
      }
      for (int j=0; j < route.size(); j++) {
        BRN_DEBUG(" - %d  %s", j, route[j].s().c_str());
      }
      BRN_DEBUG("* don't have route to %s; buffering packet.", dst_addr.s().c_str());
    }

    IPAddress dst_ip_addr;
    IPAddress src_ip_addr;

    if (!buffer_packet(p_in))
    	return;

    if (ether->ether_type == ETHERTYPE_IP) {
      const click_ip *iph = (const click_ip*)(p_in->data() + sizeof(click_ether));
      dst_ip_addr = iph->ip_dst;
      src_ip_addr = iph->ip_src;

      BRN_DEBUG(" * IP-Clients: %s --> %s", src_ip_addr.s().c_str(), dst_ip_addr.s().c_str());

      // initiate dsr route request: move this packet to output port
      start_issuing_request(dst_addr, dst_ip_addr, src_addr, src_ip_addr);
    } else { // No IP protocol

      // initiate dsr route request: move this packet to output port
      start_issuing_request(dst_addr, IPAddress(), src_addr, IPAddress());
    }
    return;
  }
}

//-----------------------------------------------------------------------------
// Timer-driven events
//-----------------------------------------------------------------------------

/* functions to handle expiration of rreqs in our forwarded_rreq map */
void
RouteQuerier::static_rreq_expire_hook(Timer *, void *v)
{
  RouteQuerier *r = (RouteQuerier *)v;
  r->rreq_expire_hook();
}

/* functions to manage the sendbuffer */
void
RouteQuerier::static_sendbuffer_timer_hook(Timer *, void *v)
{
  RouteQuerier *rt = (RouteQuerier*)v;
  rt->sendbuffer_timer_hook();
}

/* functions to downgrade blacklist entries - not yet used. */
void 
RouteQuerier::static_blacklist_timer_hook(Timer *, void *v)
{
  RouteQuerier *rt = (RouteQuerier*)v;
  rt->blacklist_timer_hook();
}

/* called by timer */
void
RouteQuerier::static_rreq_issue_hook(Timer *, void *v) 
{
  RouteQuerier *r = (RouteQuerier *)v;
  r->rreq_issue_hook();
}

//-----------------------------------------------------------------------------
// Helper methods
//-----------------------------------------------------------------------------

/* processes some checks */
void
RouteQuerier::check() 
{
  // _link_table
  assert(_link_table);

  // _blacklist
  for (BlacklistIter i = _blacklist.begin(); i.live(); i++)
    i.value().check();

  // _sendbuffer_map
  for (SBMapIter i = _sendbuffer_map.begin(); i.live(); i++) {
    SBSourceMap &src_map = i.value();

    for (SBSourceMapIter j = src_map.begin(); j.live(); j++) {
      SendBuffer &sb = j.value();
      for (int k = 0; k < sb.size(); k++) {
        sb[k].check();
      }
    }
  }

  // _forwarded_rreq_map
  for (FWReqIter i = _forwarded_rreq_map.begin(); i.live(); i++)
    i.value().check();

  // _initiated_rreq_map
  for (InitReqIter i = _initiated_rreq_map.begin(); i.live(); i++)
    i.value().check();

  // _rreq_expire_timer;
  assert(_rreq_expire_timer.scheduled());
  // _rreq_issue_timer;
  assert(_rreq_issue_timer.scheduled());
  // _sendbuffer_timer;
  assert(_sendbuffer_timer.scheduled());
  // _blacklist_timer;
  assert(_blacklist_timer.scheduled());    
}

/* flushes the send buffer */
void
RouteQuerier::flush_sendbuffer()
{

  BRN_DEBUG(" * flushing send buffer.");

  for (SBMapIter i = _sendbuffer_map.begin(); i.live(); i++) {
    SBSourceMap &src_map = i.value();

    for (SBSourceMapIter j = src_map.begin(); j.live(); j++) {
      SendBuffer &sb = j.value();
      for (int k = 0; k < sb.size(); k++) {
        sb[k].check();
        sb[k]._p->kill();
      }
      sb = SendBuffer();
    }
    src_map = SBSourceMap();
  }
}

/*
 * Buffer ordinary packet for later transmission.
 * Packet buffering is done for each source and destination seperately.
 * 
 * return false if packet was killed, else return true;
 */
bool
RouteQuerier::buffer_packet(Packet *p)
{
  p = _brn_iapp->filter_buffered_packet(p);
  if (NULL == p)
    return false;

  EtherAddress dst;
  EtherAddress src;

  const click_ether *ether = (const click_ether *)p->ether_header();

  if (!ether) {
    BRN_DEBUG(" * ether anno not available; drop packet.");
    p->kill();
    return false;
  }

  if (ether->ether_type == ETHERTYPE_BRN) {
    const click_brn_dsr *dsr = (const click_brn_dsr *)( p->data() + sizeof(click_brn) );
    dst = EtherAddress(dsr->dsr_dst.data);
    src = EtherAddress(dsr->dsr_src.data);
  } else {
    dst = EtherAddress(ether->ether_dhost);
    src = EtherAddress(ether->ether_shost);
  }
  BRN_DEBUG(" * buffering packet from %s to %s", src.s().c_str(), dst.s().c_str());

  // find the right sendbuffer for the given dst and src
  // _sendbuffer_map is a hashtable of hashtables
  SBSourceMap *src_map = _sendbuffer_map.findp(dst);

  SendBuffer *sb;
  if (!src_map) { // no entry for the requested destination
    _sendbuffer_map.insert(dst, SBSourceMap());
    src_map = _sendbuffer_map.findp(dst);
  }
  sb = src_map->findp(src);
  if (!sb) {
    src_map->insert(src, SendBuffer());
    sb = src_map->findp(src);
    BRN_DEBUG(" * buffer size = %d", sb->size());
  }

  if (sb->size() >= BRN_DSR_SENDBUFFER_MAX_LENGTH) { // TODO think about this restriction
    BRN_WARN("too many packets for host %s; killing", dst.s().c_str());
    p->kill();
	return false;
  } else {
    sb->push_back(p);

    if(_debug == BrnLogger::DEBUG) {
      SendBuffer *tmp_buff = _sendbuffer_map.findp(dst)->findp(src);
      if (tmp_buff) {
        BRN_DEBUG(" * size = %d", tmp_buff->size());
        const click_ether *tmp = (const click_ether *)tmp_buff->begin()->_p->ether_header();
        EtherAddress tmp_dst(tmp->ether_dhost);
        EtherAddress tmp_src(tmp->ether_shost);
        BRN_DEBUG("* buffering packet ... %s -> %s", tmp_src.s().c_str(), tmp_dst.s().c_str());
      } else {
        BRN_DEBUG("* buffer is null");
      }
      BRN_DEBUG("* %d packets for this host", sb->size());
    }
  }
  return true;
}

/*
 * build and send out a dsr route request.
 */
void 
RouteQuerier::issue_rreq(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip, unsigned int ttl) 
{

  BRN_DEBUG("* issue_rreq: ... #ID %d", _rreq_id);

  Packet *rreq_p = _dsr_encap->create_rreq(dst, dst_ip, src, src_ip, _rreq_id);
  //increment route request identifier
  _rreq_id++;

  Packet *brn_p = _brn_encap->add_brn_header(rreq_p, ttl);

  if (_debug) {
    RouteQuerierRoute request_route;

    _dsr_decap->extract_source_route(brn_p, request_route);
    for (int j = 0; j < request_route.size(); j++)
      BRN_DEBUG(" RREQX - %d   %s (%d)",
                    j, request_route[j].ether().s().c_str(), request_route[j]._metric);
  }

  EtherAddress bcast((const unsigned char *)"\xff\xff\xff\xff\xff\xff"); //receiver
  //set ethernet annotation header
//  brn_p->set_dst_ether_anno(bcast);

  // set ns2 anno
  brn_p->set_user_anno_c(PACKET_TYPE_KEY_USER_ANNO, BRN_DSR_RREQ);

  output(0).push(brn_p); //vorher:0
}

/*
 * start issuing requests for a host.
 */
void
RouteQuerier::start_issuing_request(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip) 
{

  // check to see if we're already querying for this host
  InitiatedReq *r = _initiated_rreq_map.findp(dst);

  if (r) {
    BRN_DEBUG(" * start_issuing_request:  already issuing requests from %s for %s",
        src.s().c_str(), dst.s().c_str());
    return;
  } else {
    // send out the initial request and add an entry to the table (_initiated_rreq_map)
    InitiatedReq new_rreq(dst, dst_ip, src, src_ip);
    _initiated_rreq_map.insert(dst, new_rreq);
    issue_rreq(dst, dst_ip, src, src_ip, BRN_DSR_RREQ_TTL1);
    return;
  }
}

/*
 * we've received a route reply.  remove the cooresponding entry from
 * route request table, so we don't send out more requests
 */
void
RouteQuerier::stop_issuing_request(EtherAddress host) 
{
  InitiatedReq *r = _initiated_rreq_map.findp(host);
  if (!r) {
    BRN_DEBUG(" * stop_issuing_request:  no entry in request table for %s", host.s().c_str());
    return;
  } else {
    _initiated_rreq_map.remove(host);
    return;
  }
}

/* called when a route request expired */
void
RouteQuerier::rreq_expire_hook() 
{

  // iterate over the _forwarded_rreq_map and remove old entries.

  // also look for requests we're waiting to forward (after a
  // unidirectionality test) and if the last hop is not in the
  // blacklist, then forward it.  if it's been a while since we issued
  // the direct request, kill the packet.
  //
  // give up on the unidirectionality test (and kill the packet) after
  // DSR_RREQ_UNITEST_TIMEOUT.  otherwise we might end up forwarding
  // the request long after it came to us.

  // we also kill the packet if we positively determined
  // unidirectionality -- e.g. if we get a tx error forwarding a route
  // reply in the meantime.

  struct timeval curr_time;
  click_gettimeofday(&curr_time);

  Vector<ForwardedReqKey> remove_list;
  for (FWReqIter i = _forwarded_rreq_map.begin(); i.live(); i++) {

    ForwardedReqVal &val = i.value();

    if (val.p != NULL) { // we issued a unidirectionality test
      int last_hop_metric;
      RouteQuerierRoute req_route;

      _dsr_decap->extract_request_route(val.p, &last_hop_metric, req_route);

      EtherAddress last_forwarder = req_route[req_route.size() - 1].ether();
      EtherAddress last_eth = last_forwarder_eth(val.p);

      int status = check_blacklist(last_forwarder);
      if (status == BRN_DSR_BLACKLIST_NOENTRY) { // reply came back
        BRN_DEBUG("* unidirectionality test succeeded; forwarding route request.");
//        forward_rreq(val.p);
        output(0).push(val.p); // forward packet to request forwarder, vorher:1
	
	// a) we cannot issue a unidirectionality test if there is an
	// existing metric
	//
	// b) if, after we issue a test, this RREQ comes over a
	// different link, with a valid metric, and we forward it,
	// then we essentially cancel the unidirectionality test.
	//
	// so we know that if the test comes back positive we can just
	// just calculate the route metric and call that best.
        // TODO think about DSRHop; why eth1 address?
//TODO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//	req_route.push_back(RouteQuerierHop(*me_eth1, get_metric(last_eth)));
//TODO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	val.best_metric = route_metric(req_route);

        val.p = NULL;
      } else if ((status == BRN_DSR_BLACKLIST_UNI_PROBABLE) ||
		 (diff_in_ms(curr_time, val._time_unidtest_issued) > BRN_DSR_BLACKLIST_UNITEST_TIMEOUT)) {
        BRN_DEBUG("* unidirectionality test failed; killing route request.");
	val.p->kill();
	val.p = NULL;
      }
    }
/*
     click_chatter("i.key is %s %s %d %d\n", i.key()._src.s().c_str(), 
 		  i.key()._target.s().c_str(), i.key()._id,
 		  diff_in_ms(curr_time, i.value()._time_forwarded));
*/
    if (diff_in_ms(curr_time, i.value()._time_forwarded) > BRN_DSR_RREQ_TIMEOUT) {
      EtherAddress src(i.key()._src);
      EtherAddress dst(i.key()._target);
      unsigned int id = i.key()._id;
      BRN_DEBUG(" RREQ entry has expired; %s -> %s (%d)", src.s().c_str(), dst.s().c_str(), id);

      remove_list.push_back(i.key());
    }
  }
  for (int i = 0; i < remove_list.size(); i++) {
    _forwarded_rreq_map.remove(remove_list[i]);
  }

  _rreq_expire_timer.schedule_after_msec(BRN_DSR_RREQ_EXPIRE_TIMER_INTERVAL);

  check();
}

void
RouteQuerier::sendbuffer_timer_hook()
{

  struct timeval curr_time;
  click_gettimeofday(&curr_time);

  int total = 0; // total packets sent this scheduling
  bool check_next_time = false;

  for (SBMapIter i = _sendbuffer_map.begin(); i.live(); i++) {

    SBSourceMap &src_map = i.value();
    EtherAddress dst = i.key();

    for (SBSourceMapIter x = src_map.begin(); x.live(); x++) {

      SendBuffer &sb = x.value();
      EtherAddress src = x.key();

      if (! sb.size()) {
        continue;
      }

      //TODO: _sendbuffer_check_routes should be adjusted by link probing
      //if (_sendbuffer_check_routes) {  // have we received a route reply recently?

        // XXX should we check for routes for packets in the sendbuffer
        // every time we update the link cache (i.e. if we forward
        // someone else's route request/reply and add new entries to our
        // cache?  right now we only check if we receive a route reply
        // directed to us.
        if(_debug == BrnLogger::DEBUG)
          BRN_DEBUG(" * send buffer has %d packet%s with source %s and destination %s",
                        sb.size(),
                        sb.size() == 1 ? "" : "s",
                        src.s().c_str(),
                        dst.s().c_str());

        // search route for destination in the link cache first
        int metric_of_route = -1;
        EtherAddresses route;
        EtherAddresses* fixed_route = fixed_routes.findp(EtherPair(dst, src));
        if ( fixed_route && fixed_route->size() >= 2 && 
          ((*fixed_route)[0] == dst) && ((*fixed_route)[fixed_route->size() - 1] == src) ) {
          BRN_DEBUG(" * Using fixed route.");
          route = *fixed_route;
          metric_of_route = 1;
        } else {
	        _link_table->query_route( src, dst, route );
          metric_of_route = _link_table->get_route_metric(route);
        }

        

        if ( ( route.size() > 1 )  && ( metric_of_route != -1 ) ) { // route available

          if(_debug == BrnLogger::DEBUG) {
            BRN_DEBUG(" * have a route:");
            for (int j=0; j<route.size(); j++) {
              BRN_DEBUG(" SRC - %d  %s",
                          j, route[j].s().c_str());
            }
          }

          if (total < BRN_DSR_SENDBUFFER_MAX_BURST) {

            int k;
            for (k = 0; k < sb.size() && total < BRN_DSR_SENDBUFFER_MAX_BURST; k++, total++) {
              // send out each of the buffered packets
              Packet *p = sb[k]._p;
              //Packet *p_out = add_dsr_header(p, route);
              Packet *p_out = _dsr_encap->add_src_header(p, route); //todo brn_dsr packet --> encap
              // add BRN header
              Packet *brn_p = _brn_encap->add_brn_header(p_out);

              output(1).push(brn_p);
            }

            if (k < sb.size())
              check_next_time = true; // we still have packets with a route

            SendBuffer new_sb;
            for ( ; k < sb.size() ; k++) {
              // push whatever packets we didn't send onto new_sb, then
              // replace the existing sendbuffer
              new_sb.push_back(sb[k]._p);
            }
            sb = new_sb;
          }
          // go to the next destination's sendbuffer; we don't check for
          // expired packets if there is a route for that host
          continue;
        } else {
          BRN_DEBUG(" * still no route to %s", dst.s().c_str());
        }
      //}

      // if we're here, either check_routes was false (and we're being
      // called because the timer expired) or there was no route

      SendBuffer new_sb;
      for (int j = 0; j < sb.size(); j++) {
        unsigned long time_elapsed = diff_in_ms(curr_time, sb[j]._time_added);

        if (time_elapsed >= BRN_DSR_SENDBUFFER_TIMEOUT) {
          BRN_DEBUG(" * packet %d expired in send buffer", j);
          sb[j]._p->kill();
        } else {
          // click_chatter(" * packet %d gets to stay\n", i);
          new_sb.push_back(sb[j]);
        }
      }
      sb = new_sb;
      if (sb.size() == 0) {
        // if we expire the last packet in the sendbuffer with this
        // destination, stop sending RREQs
        stop_issuing_request(dst);
      }
    }
  }

  _sendbuffer_check_routes = check_next_time;
  _sendbuffer_timer.schedule_after_msec(BRN_DSR_SENDBUFFER_TIMER_INTERVAL);

  check();
}

void 
RouteQuerier::blacklist_timer_hook()
{
  timeval curr_time;
  click_gettimeofday(&curr_time);

  for (BlacklistIter i = _blacklist.begin(); i.live(); i++) {
    if ((i.value()._status == BRN_DSR_BLACKLIST_UNI_PROBABLE) &&
	(diff_in_ms(curr_time, i.value()._time_updated) > BRN_DSR_BLACKLIST_ENTRY_TIMEOUT)) {

      BlacklistEntry &e = i.value();
      BRN_DEBUG(" * downgrading blacklist entry for host %s", i.key().s().c_str());
      e._status = BRN_DSR_BLACKLIST_UNI_QUESTIONABLE;
    }
  }
  _blacklist_timer.schedule_after_msec(BRN_DSR_BLACKLIST_TIMER_INTERVAL);

  check();
}

/* */
void 
RouteQuerier::rreq_issue_hook() 
{
  // look through the initiated rreqs and check if it's time to send
  // anything out
  timeval curr_time;
  click_gettimeofday(&curr_time);

  EtherAddresses remove_list;

  for (InitReqIter i = _initiated_rreq_map.begin(); i.live(); i++) {

    InitiatedReq &ir = i.value();
    assert(ir._target == i.key());

    // we could find out a route by some other means than a direct
    // RREP.  if this is the case, stop issuing requests.
    EtherAddresses route;
    _link_table->query_route( ir._source, ir._target, route );

    int metric_of_route = _link_table->get_route_metric(route);

    if(_debug == BrnLogger::DEBUG) {
      BRN_DEBUG(" BUGTRACK: * Metric for route is %d\n * Route is", metric_of_route);

      for (int j=0; j < route.size(); j++) {
        BRN_DEBUG(" - %d  %s", j, route[j].s().c_str());
      }
    }

    if ( ( route.size() > 1 )  && ( metric_of_route != -1 ) ) { // route available
      remove_list.push_back(ir._target);
      continue;
    } else {
      if (diff_in_ms(curr_time, ir._time_last_issued) > ir._backoff_interval) {
        BRN_DEBUG(" * time to issue new request for host %s", ir._target.s().c_str());
	
	if (ir._times_issued == 1) {
	  // if this is the second request
	  ir._backoff_interval = BRN_DSR_RREQ_DELAY2;
	} else {
	  ir._backoff_interval *= BRN_DSR_RREQ_BACKOFF_FACTOR;
	  // i don't think there's any mention in the spec of a limit on
	  // the backoff, but this MAX_DELAY seems reasonable
	  if (ir._backoff_interval > BRN_DSR_RREQ_MAX_DELAY)
	    ir._backoff_interval = BRN_DSR_RREQ_MAX_DELAY;
	}
	ir._times_issued++;
	ir._time_last_issued = curr_time;
	ir._ttl = BRN_DSR_RREQ_TTL2;
	
	//issue_rreq(ir._target, ir._ttl, false); AZu: vorher
        issue_rreq(ir._target, ir._target_ip, ir._source, ir._source_ip, ir._ttl);
      }
    }
  }

  for (int j = 0 ; j < remove_list.size() ; j++) {
    _initiated_rreq_map.remove(remove_list[j]);
  }

  _rreq_issue_timer.schedule_after_msec(BRN_DSR_RREQ_ISSUE_TIMER_INTERVAL);

  check();
}

//-----------------------------------------------------------------------------
// Utility methods
//-----------------------------------------------------------------------------

int 
RouteQuerier::check_blacklist(EtherAddress ether) 
{
  if (!_use_blacklist) return BRN_DSR_BLACKLIST_NOENTRY;

  BlacklistEntry *e = _blacklist.findp(ether);
  if (! e) {
    return BRN_DSR_BLACKLIST_NOENTRY;
  } else {
    return e->_status;
  }
}

void
RouteQuerier::set_blacklist(EtherAddress ether, int s)
{

  BRN_DEBUG(" set blacklist: %s %d", ether.s().c_str(), s);
  BRN_DEBUG(" set blacklist: %d", check_blacklist(ether));

  _blacklist.remove(ether);
  if (s != BRN_DSR_BLACKLIST_NOENTRY) {
    BlacklistEntry e;
    click_gettimeofday(&(e._time_updated));
    e._status = s;
    _blacklist.insert(ether, e);
  }

  BRN_DEBUG(" set blacklist: %d", check_blacklist(ether));
}

unsigned long
RouteQuerier::diff_in_ms(timeval t1, timeval t2)
{
  unsigned long s, us, ms;

//  click_chatter("diff_in_ms: t1 %ld %ld; t2 %ld %ld\n",
//      t1.tv_sec, t1.tv_usec, t2.tv_sec, t2.tv_usec);

  while (t1.tv_usec < t2.tv_usec) {
    t1.tv_usec += 1000000;
    t1.tv_sec -= 1;
  }

  assert(t1.tv_sec >= t2.tv_sec);

  s = t1.tv_sec - t2.tv_sec;
  assert(s < ((unsigned long)(1<<31))/1000);

  us = t1.tv_usec - t2.tv_usec;
  ms = s * 1000L + us / 1000L;

//  click_chatter("diff_in_ms: difference is %ld %ld -> %ld\n",
// 		 s, us, ms);

  return ms;
}

// Ask LinkStat for the metric for the link from other to us.
// ripped off from srcr.cc
unsigned char
RouteQuerier::get_metric(EtherAddress)
{

  BRN_DEBUG(" * get_metric: not yet implemented --> TODO !!!!!!!!!!!");
  return 1;
#if 0
  unsigned char dft = DSR_INVALID_HOP_METRIC; // default metric
  if (_ls){
    unsigned int tau;
    struct timeval tv;
    unsigned int frate, rrate;
    bool res = _ls->get_forward_rate(other, &frate, &tau, &tv);
    if(res == false) {
      return dft;
    }
    res = _ls->get_reverse_rate(other, &rrate, &tau);
    if (res == false) {
      return dft;
    }
    if (frate == 0 || rrate == 0) {
      return dft;
    }

    // rate is 100 * recv %
    // m = 10 x 1/(fwd% x rev%)
    u_short m = 10 * 100 * 100 / (frate * (int) rrate);

    if (m > DSR_INVALID_HOP_METRIC) {
      if(_debug)
        click_chatter("%d.%d (%s,%s) DSRRouteTable::get_metric: metric too big for one byte?\n");
      return DSR_INVALID_HOP_METRIC;
    }

    return (unsigned char)m;
  } 
#endif
/*
  if (_metric) {
    GenericMetric::metric_t m = _metric->get_link_metric(other, false);
    unsigned char c = _metric->scale_to_char(m);
    if (!m.good() || c >= DSR_INVALID_HOP_METRIC)
      return DSR_INVALID_HOP_METRIC;
    return c;
  }
  else {
    // default to hop-count, all links have a hop-count of 1
    return 1; 
  }
*/
}

bool
RouteQuerier::metric_preferable(unsigned short a, unsigned short b) 
{
  if (!_metric)
    return (a < b); // fallback to minimum hop-count
//TODO: AZU use real metric here
/*
  else if (a == BRN_DSR_INVALID_ROUTE_METRIC || b == BRN_DSR_INVALID_ROUTE_METRIC)
    return a; // make arbitrary choice
  else
    return _metric->metric_val_lt(_metric->unscale_from_char(a),
				  _metric->unscale_from_char(b));
*/
  return true;
}

unsigned short
RouteQuerier::route_metric(RouteQuerierRoute r)
{
#if 0
  unsigned short ret = 0;
  // the metric in r[i+1] represents the link between r[i] and r[i+1],
  // so we start at 1
  for (int i = 1; i < r.size(); i++) {
    if (r[i]._metric == DSR_INVALID_HOP_METRIC) 
      return DSR_INVALID_ROUTE_METRIC;
    ret += r[i]._metric;
  }
  return ret;
#endif

  if (r.size() < 2) {
    BRN_WARN(" route_metric: route is too short, less than two nodes????");
    return BRN_DSR_INVALID_ROUTE_METRIC;
  }
  if (!_metric) {
    BRN_DEBUG(" * fallback to hop-count: %d", r.size());
    return r.size(); // fallback to hop-count
  } else {
    BRN_DEBUG("* use route metric ...");
  }

  if (r[1]._metric == BRN_DSR_INVALID_HOP_METRIC)
    return BRN_DSR_INVALID_ROUTE_METRIC;
/*
  GenericMetric::metric_t m(_metric->unscale_from_char(r[1]._metric));

  for (int i = 2; i < r.size(); i++) {
     if (r[i]._metric == BRN_DSR_INVALID_HOP_METRIC) 
      return BRN_DSR_INVALID_ROUTE_METRIC;
     m = _metric->append_metric(m, _metric->unscale_from_char(r[i]._metric));
  }
  if (m.good())
    return _metric->scale_to_char(m);
  else
    return BRN_DSR_INVALID_ROUTE_METRIC;
*/
  return BRN_DSR_INVALID_ROUTE_METRIC;
}


EtherAddress
RouteQuerier::last_forwarder_eth(Packet *p)
{
  // TODO !!!!!!!!!!!!!!!!!!!
  // what going on here??????????
  uint16_t d[3];
  d[0] = p->user_anno_us(9);
  d[1] = p->user_anno_us(11);
  d[2] = p->user_anno_us(13);
  
  return (EtherAddress((unsigned char *)d));
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {H_DEBUG, H_FIXED_ROUTE, H_FIXED_ROUTE_CLEAR, H_FLUSH_SB};

static String
read_handler(Element *e, void * vparam)
{
  RouteQuerier *rq = (RouteQuerier *)e;

  switch ((intptr_t)vparam) {
    case H_DEBUG: {
      return String(rq->_debug) + "\n";
    }
    case H_FIXED_ROUTE: {
      String ret;
      for (RouteQuerier::RouteMap::const_iterator iter = rq->fixed_routes.begin(); 
          iter.live(); ++iter) {
        const RouteQuerier::EtherPair& pair = iter.key();
        ret += pair.first.s() + " -> " + pair.second.s() + ":\n";
        
        const EtherAddresses& route = iter.value();
        for (int i = 0; i < route.size(); i++) {
          const EtherAddress& addr = route.at(i);
          ret += "    " + addr.s() + "\n";
        }
          ret += "\n";
      }
      return ret;
    }
  }
  
  return String("n/a\n");
}

static int 
write_handler(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  RouteQuerier *rq = (RouteQuerier *)e;
  String s = cp_uncomment(in_s);

  switch ((intptr_t)vparam) {
    case H_DEBUG: {
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("debug parameter must be an integer value between 0 and 4");
      rq->_debug = debug;
      break;
    }
    case H_FIXED_ROUTE: {
      click_chatter("fixed_route called\n");
      EtherAddresses routeForward, routeReverse;
      
      StringTokenizer tokenizer(s);
      String token;
      
      while( tokenizer.hasMoreTokens() ) {
        token = tokenizer.getNextToken();
        cp_eat_space(token);
        if (token == "")
          continue;
        EtherAddress m;
        if (!cp_ethernet_address(token, &m)) 
          return errh->error("fixed_route parameter must be etheraddress:%s.", token.c_str());
        click_chatter("add ether %s to route\n", m.s().c_str());
        routeForward.push_back(m);
        routeReverse.insert(routeReverse.begin(), m);
      }
      
      int size = routeForward.size();
      if (size < 2)
        return errh->error("fixed_route must contain at least 2 entries.");
      
      RouteQuerier::EtherPair keyForward(routeForward[0], routeForward[size-1]);
      rq->fixed_routes.insert(keyForward, routeForward);

      RouteQuerier::EtherPair keyReverse(routeReverse[0], routeReverse[size-1]);
      rq->fixed_routes.insert(keyReverse, routeReverse);

      break;
    }
    case H_FIXED_ROUTE_CLEAR: {
      rq->fixed_routes.clear();
      break;
    }
    case H_FLUSH_SB: {
      bool to_flush;
      if (!cp_bool(s, &to_flush)) 
        return errh->error("flush_sb parameter must be a boolean.");
      if (to_flush)
        rq->flush_sendbuffer();
      break;
    }
  }
  return 0;
}

void
RouteQuerier::add_handlers()
{
  add_read_handler("debug", read_handler, (void*) H_DEBUG);
  add_read_handler("fixed_route", read_handler, (void*) H_FIXED_ROUTE);

  add_write_handler("debug", write_handler, (void*) H_DEBUG);
  add_write_handler("fixed_route", write_handler, (void*) H_FIXED_ROUTE);
  add_write_handler("fixed_route_clear", write_handler, (void*) H_FIXED_ROUTE_CLEAR);
  add_write_handler("flush_sb", write_handler, (void*) H_FLUSH_SB);
}


ELEMENT_REQUIRES(LinkTable)
EXPORT_ELEMENT(RouteQuerier)

#include <click/bighashmap.cc>
#include <click/vector.cc>
template class HashMap<ForwardedReqKey, ForwardedReqVal>;
template class HashMap<EtherAddress, RouteQuerier::InitiatedReq>;
template class Vector<RouteQuerier::BufferedPacket>;
template class HashMap<EtherAddress, RouteQuerier::SendBuffer>;
template class HashMap<EtherAddress, RouteQuerier::BlacklistEntry>;
template class HashMap<RouteQuerier::EtherPair,EtherAddresses>;

CLICK_ENDDECLS
