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

#include "elements/brn2/routing/linkstat/metric/brn2_genericmetric.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "brn2_routequerier.hh"

CLICK_DECLS

/* constructor initalizes timer, ... */
BRN2RouteQuerier::BRN2RouteQuerier()
  : _sendbuffer_check_routes(false),
    _sendbuffer_timer(static_sendbuffer_timer_hook, this),
    _me(NULL),
    _link_table(),
    _dsr_encap(),
    _dsr_decap(),
    _dsr_rid_cache(NULL),
    _rid_ac(0),
    _rreq_expire_timer(static_rreq_expire_hook, this),
    _rreq_issue_timer(static_rreq_issue_hook, this),
    _blacklist_timer(static_blacklist_timer_hook, this),
    _metric(0),
    _use_blacklist(false),
    _expired_packets(0),
    _requests(0),
    _max_retries(-1)
{
  BRNElement::init();

  timeval tv = Timestamp::now().timeval();
}

/* destructor processes some cleanup */
BRN2RouteQuerier::~BRN2RouteQuerier()
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
BRN2RouteQuerier::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      "ROUTEMAINTENANCE", cpkP+cpkM, cpElement, &_routing_maintenance,
      "DSRENCAP",  cpkP+cpkM, cpElement, &_dsr_encap,
      "DSRDECAP", cpkP+cpkM, cpElement, &_dsr_decap,
      "DSRIDCACHE", cpkP, cpElement, &_dsr_rid_cache,
      "METRIC", cpkP, cpElement, &_metric,
      "USE_BLACKLIST", cpkP, cpBool, &_use_blacklist,
      "MAX_RETRIES", cpkP, cpInteger, &_max_retries,
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

/* initializes error handler */
int
BRN2RouteQuerier::initialize(ErrorHandler */*errh*/)
{
  click_srandom(_me->getMasterAddress()->hashcode());

  _rreq_id = click_random() % 0xffff;

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
BRN2RouteQuerier::uninitialize()
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
BRN2RouteQuerier::push(int, Packet *p_in)
{
  EtherAddresses route;

  if ( p_in->length() < sizeof(click_ether) ) {
    BRN_DEBUG(" * packet too small to be etherframe; drop packet.");
    p_in->kill();
    return;
  }

  click_ether *ether = (click_ether *)p_in->data();//better to use this, since ether_header is not always set.it also can be overwriten
  uint8_t p_in_ttl = BRNPacketAnno::ttl_anno(p_in);
  if ( p_in_ttl == 0) p_in_ttl = 255; //TODO: ttl = route len

  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  if (!dst_addr || !src_addr) {
    BRN_ERROR(" ethernet anno header is null; kill packet.");
    //PrintEtherAnno::print("bogus packet.", p_in);
    p_in->kill();
    return;
  }

  uint16_t last_hop_metric = _link_table->get_host_metric_to_me(src_addr);
  if ( !(_me->isIdentical(&src_addr) || _link_table->is_associated(src_addr) || (last_hop_metric > 0) ) ) {
    //TODO: fix it
    BRN_INFO(" unknown src of packet; kill packet.");
    p_in->kill();
    return;
  }

  uint32_t metric_of_route = 0;
  EtherAddresses* fixed_route = fixed_routes.findp(EtherPair(dst_addr, src_addr));

  if ( fixed_route && (fixed_route->size() >= 2)
    && ((*fixed_route)[0] == dst_addr)
    && ((*fixed_route)[fixed_route->size() - 1] == src_addr) ) {
      BRN_DEBUG("* Using fixed route. %s, (%s), %s, (%s)", 
        (*fixed_route)[0].unparse().c_str(), dst_addr.unparse().c_str(),
        (*fixed_route)[fixed_route->size() - 1].unparse().c_str(), src_addr.unparse().c_str());
    route = *fixed_route;
    metric_of_route = 1;  //Don't care about the metric. Just set to 1 to mark it available

  } else {
    BRN_DEBUG(" query route: %s (src) to %s (dst)",src_addr.unparse().c_str(),dst_addr.unparse().c_str());
    _routing_maintenance->query_route( src_addr, dst_addr, route, &metric_of_route );
    metric_of_route = _routing_maintenance->get_route_metric(route); //update metric if lanks have changed
  }

  BRN_DEBUG(" Metric of found route = %d route_length = %d", metric_of_route, route.size());

  if(_debug == BrnLogger::DEBUG) {
    String route_str = _routing_maintenance->print_routes(true);
    BRN_DEBUG(" * routestr:\n%s", route_str.c_str());
  }

  if ( ( route.size() > 1 ) && ( metric_of_route != 0 ) ) { // route available
    if(_debug == BrnLogger::DEBUG) {
      BRN_DEBUG(" * have cached route:");
      for (int j=0; j < route.size(); j++) {
        BRN_DEBUG(" - %d  %s", j, route[j].unparse().c_str());
      }
    }

    // add DSR headers to packet..
    Packet *dsr_p = _dsr_encap->add_src_header(p_in, route); //todo brn_dsr packet --> encap

    uint16_t _path_id = 0;
    BrnRouteIdCache::RouteIdEntry* rid_e;
    if ( _dsr_rid_cache ) {
      rid_e = _dsr_rid_cache->get_entry(&src_addr, &dst_addr);
      if ( rid_e ) {
        _path_id = rid_e->_id;
        rid_e->update();
      } else {
        _rid_ac++;
        _path_id = _rid_ac; 
        rid_e = _dsr_rid_cache->insert_entry(&src_addr, &dst_addr, &src_addr, &(route[route.size() - 2]) , _path_id);
      }

      click_brn_dsr *dsr_source = (click_brn_dsr *)(dsr_p->data());
      dsr_source->dsr_id = htons(_path_id);
    }

   // add BRN header
    Packet *brn_p = BRNProtocol::add_brn_header(dsr_p, BRN_PORT_DSR, BRN_PORT_DSR, p_in_ttl,
                                                BRNPacketAnno::tos_anno(dsr_p));

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
        BRN_DEBUG(" - %d  %s", j, route[j].unparse().c_str());
      }
      BRN_DEBUG("* don't have route to %s; buffering packet.", dst_addr.unparse().c_str());
    }

    IPAddress dst_ip_addr;
    IPAddress src_ip_addr;

    if (!buffer_packet(p_in))
      return;

    if (ether->ether_type == ETHERTYPE_IP) {
      const click_ip *iph = (const click_ip*)(p_in->data() + sizeof(click_ether));
      dst_ip_addr = iph->ip_dst;
      src_ip_addr = iph->ip_src;

      BRN_DEBUG(" * IP-Clients: %s --> %s", src_ip_addr.unparse().c_str(), dst_ip_addr.unparse().c_str());

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
BRN2RouteQuerier::static_rreq_expire_hook(Timer *, void *v)
{
  BRN2RouteQuerier *r = (BRN2RouteQuerier *)v;
  r->rreq_expire_hook();
}

/* functions to manage the sendbuffer */
void
BRN2RouteQuerier::static_sendbuffer_timer_hook(Timer *, void *v)
{
  BRN2RouteQuerier *rt = (BRN2RouteQuerier*)v;
  rt->sendbuffer_timer_hook();
}

/* functions to downgrade blacklist entries - not yet used. */
void 
BRN2RouteQuerier::static_blacklist_timer_hook(Timer *, void *v)
{
  BRN2RouteQuerier *rt = (BRN2RouteQuerier*)v;
  rt->blacklist_timer_hook();
}

/* called by timer */
void
BRN2RouteQuerier::static_rreq_issue_hook(Timer *, void *v)
{
  BRN2RouteQuerier *r = (BRN2RouteQuerier *)v;
  r->rreq_issue_hook();
}

//-----------------------------------------------------------------------------
// Helper methods
//-----------------------------------------------------------------------------

/* processes some checks */
void
BRN2RouteQuerier::check()
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
BRN2RouteQuerier::flush_sendbuffer()
{

//  BRN_DEBUG(" * flushing send buffer.");

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
BRN2RouteQuerier::buffer_packet(Packet *p)
{
  EtherAddress dst;
  EtherAddress src;

  const click_ether *ether = (const click_ether *)p->data();//better to use this, since ether_header is not always set.it also can be overwriten
  //const click_ether *ether = (const click_ether *)p->ether_header();

  if (ether->ether_type == ETHERTYPE_BRN) {
    const click_brn_dsr *dsr = (const click_brn_dsr *)( p->data() + sizeof(click_brn) );  //TODO: BRN-packet in queue is alway dsr ??
    dst = EtherAddress(dsr->dsr_dst.data);
    src = EtherAddress(dsr->dsr_src.data);
    click_chatter("BRN in queue-----------------------------------------------------------------------------------");
  } else {
    dst = EtherAddress(ether->ether_dhost);
    src = EtherAddress(ether->ether_shost);
  }

  BRN_DEBUG(" * buffering packet from %s to %s", src.unparse().c_str(), dst.unparse().c_str());

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
    BRN_WARN("too many packets for host %s from host %s; killing", dst.unparse().c_str(), src.unparse().c_str() );
    p->kill();
    return false;
  } else {
    sb->push_back(p);

    if(_debug == BrnLogger::DEBUG) {
      SendBuffer *tmp_buff = _sendbuffer_map.findp(dst)->findp(src);
      if (tmp_buff) {
        BRN_DEBUG(" * size = %d", tmp_buff->size());
        const click_ether *tmp = (const click_ether *)tmp_buff->begin()->_p->data();
        EtherAddress tmp_dst(tmp->ether_dhost);
        EtherAddress tmp_src(tmp->ether_shost);
        BRN_DEBUG("* buffering packet ... %s -> %s", tmp_src.unparse().c_str(), tmp_dst.unparse().c_str());
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
BRN2RouteQuerier::issue_rreq(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip, unsigned int ttl)
{

  BRN_DEBUG("* issue_rreq: ... #ID %d", _rreq_id);

  _requests++;

  Packet *rreq_p = _dsr_encap->create_rreq(dst, dst_ip, src, src_ip, _rreq_id);
  //increment route request identifier
  _rreq_id++;

  Packet *brn_p = BRNProtocol::add_brn_header(rreq_p, BRN_PORT_DSR, BRN_PORT_DSR, ttl, BRNPacketAnno::tos_anno(rreq_p));

  if (_debug) {
    BRN2RouteQuerierRoute request_route;

    _dsr_decap->extract_source_route(brn_p, request_route);
    for (int j = 0; j < request_route.size(); j++)
      BRN_DEBUG(" RREQX - %d   %s (%d)",
                    j, request_route[j].ether().unparse().c_str(), request_route[j]._metric);
  }

  EtherAddress bcast((const unsigned char *)"\xff\xff\xff\xff\xff\xff"); //receiver

  //set ethernet annotation header
  BRNPacketAnno::set_src_ether_anno(brn_p, EtherAddress()); //Set to everything, go through elements
                                                            //which duplicates packet for each device
  BRNPacketAnno::set_dst_ether_anno(brn_p, bcast);
  BRNPacketAnno::set_ethertype_anno(brn_p, ETHERTYPE_BRN);

  output(0).push(brn_p); //vorher:0
}

/*
 * start issuing requests for a host.
 */
void
BRN2RouteQuerier::start_issuing_request(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip)
{

  // check to see if we're already querying for this host
  InitiatedReq *r = _initiated_rreq_map.findp(dst);

  if (r) {
    BRN_DEBUG(" * start_issuing_request:  already issuing requests from %s for %s",
        src.unparse().c_str(), dst.unparse().c_str());
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
BRN2RouteQuerier::stop_issuing_request(EtherAddress host)
{
  InitiatedReq *r = _initiated_rreq_map.findp(host);
  if (!r) {
    BRN_DEBUG(" * stop_issuing_request:  no entry in request table for %s", host.unparse().c_str());
    return;
  } else {
    _initiated_rreq_map.remove(host);
    return;
  }
}

/* called when a route request expired */
void
BRN2RouteQuerier::rreq_expire_hook()
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
  curr_time = Timestamp::now().timeval();

  Vector<ForwardedReqKey> remove_list;
  for (FWReqIter i = _forwarded_rreq_map.begin(); i.live(); i++) {

    ForwardedReqVal &val = i.value();

    if (val.p != NULL) { // we issued a unidirectionality test
      BRN2RouteQuerierRoute req_route;

      uint8_t devicenumber = BRNPacketAnno::devicenumber_anno(val.p);
      BRN2Device *indev = _me->getDeviceByNumber(devicenumber);
      const EtherAddress *device_addr = indev->getEtherAddress();//ether addr of the interface the packet is coming from

      const click_ether *ether = (const click_ether *)val.p->ether_header();
     // rreq received from this node (last hop)
      EtherAddress prev_node(ether->ether_shost);
      uint16_t last_hop_metric = _link_table->get_link_metric(prev_node, *device_addr);
      uint16_t metric_of_route = _dsr_decap->extract_request_route(val.p, req_route, &prev_node, last_hop_metric);

      EtherAddress last_forwarder = req_route[req_route.size() - 1].ether();

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

        val.best_metric = route_metric(req_route);

        click_chatter("Route_metric: %d  metric_of_route: %d", val.best_metric, metric_of_route);
        //val.best_metric = metric_of_route;

        val.p = NULL;
      } else if ((status == BRN_DSR_BLACKLIST_UNI_PROBABLE) ||
                (diff_in_ms(curr_time, val._time_unidtest_issued) > BRN_DSR_BLACKLIST_UNITEST_TIMEOUT)) {
        BRN_DEBUG("* unidirectionality test failed; killing route request.");
        val.p->kill();
        val.p = NULL;
      }
    }
/*
      click_chatter("i.key is %s %s %d %d\n", i.key()._src.unparse().c_str(), 
      i.key()._target.unparse().c_str(), i.key()._id,
      diff_in_ms(curr_time, i.value()._time_forwarded));
*/
    if (diff_in_ms(curr_time, i.value()._time_forwarded) > BRN_DSR_RREQ_TIMEOUT) {
      EtherAddress src(i.key()._src);
      EtherAddress dst(i.key()._target);
      unsigned int id = i.key()._id;
      BRN_DEBUG(" RREQ entry has expired; %s -> %s (%d)", src.unparse().c_str(), dst.unparse().c_str(), id);

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
BRN2RouteQuerier::sendbuffer_timer_hook()
{

  struct timeval curr_time;
  curr_time = Timestamp::now().timeval();

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
                        src.unparse().c_str(),
                        dst.unparse().c_str());

        // search route for destination in the link cache first
        uint32_t metric_of_route = 0;
        EtherAddresses route;
        EtherAddresses* fixed_route = fixed_routes.findp(EtherPair(dst, src));
        if ( fixed_route && fixed_route->size() >= 2 &&
          ((*fixed_route)[0] == dst) && ((*fixed_route)[fixed_route->size() - 1] == src) ) {
          BRN_DEBUG(" * Using fixed route.");
          route = *fixed_route;
          metric_of_route = 1;
        } else {
          _routing_maintenance->query_route( src, dst, route, &metric_of_route );
          metric_of_route = _routing_maintenance->get_route_metric(route);    //update metric if links have changed
        }

        if ( ( route.size() > 1 )  && ( metric_of_route != 0 ) ) { // route available

          if(_debug == BrnLogger::DEBUG) {
            BRN_DEBUG(" * have a route:");
            for (int j=0; j<route.size(); j++) {
              BRN_DEBUG(" SRC - %d  %s",
                          j, route[j].unparse().c_str());
            }
          }

          /* For ID Routing*/
          uint16_t _path_id = 0;
          BrnRouteIdCache::RouteIdEntry* rid_e;

          if ( _dsr_rid_cache ) {
            rid_e = _dsr_rid_cache->get_entry(&src, &dst);
            if ( rid_e ) {
              _path_id = rid_e->_id;
            } else {
              _rid_ac++;
              _path_id = _rid_ac;
              rid_e = _dsr_rid_cache->insert_entry(&src, &dst, &src, &(route[route.size() - 2]) , _path_id);
            }
          }
          /*End Id Routing*/

          if (total < BRN_DSR_SENDBUFFER_MAX_BURST) {

            int k;
            for (k = 0; k < sb.size() && total < BRN_DSR_SENDBUFFER_MAX_BURST; k++, total++) {
              // send out each of the buffered packets
              Packet *p = sb[k]._p;
              Packet *p_out = _dsr_encap->add_src_header(p, route); //todo brn_dsr packet --> encap

              if ( _dsr_rid_cache ) {
                click_brn_dsr *dsr_source = (click_brn_dsr *)(p_out->data());
                dsr_source->dsr_id = htons(_path_id);
                rid_e->update();
              }

              // add BRN header
              uint8_t p_in_ttl = BRNPacketAnno::ttl_anno(p_out);
              if ( p_in_ttl == 0) p_in_ttl = 255; //TODO: ttl = route len
              Packet *brn_p = BRNProtocol::add_brn_header(p_out, BRN_PORT_DSR, BRN_PORT_DSR, p_in_ttl,
                                                          BRNPacketAnno::tos_anno(p_out));

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
          BRN_DEBUG(" * still no route to %s", dst.unparse().c_str());
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
          _expired_packets++;
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
BRN2RouteQuerier::blacklist_timer_hook()
{
  timeval curr_time;
  curr_time = Timestamp::now().timeval();

  for (BlacklistIter i = _blacklist.begin(); i.live(); i++) {
    if ((i.value()._status == BRN_DSR_BLACKLIST_UNI_PROBABLE) &&
       (diff_in_ms(curr_time, i.value()._time_updated) > BRN_DSR_BLACKLIST_ENTRY_TIMEOUT)) {

      BlacklistEntry &e = i.value();
      BRN_DEBUG(" * downgrading blacklist entry for host %s", i.key().unparse().c_str());
      e._status = BRN_DSR_BLACKLIST_UNI_QUESTIONABLE;
    }
  }
  _blacklist_timer.schedule_after_msec(BRN_DSR_BLACKLIST_TIMER_INTERVAL);

  check();
}

/* */
void 
BRN2RouteQuerier::rreq_issue_hook()
{
  // look through the initiated rreqs and check if it's time to send
  // anything out
  timeval curr_time;
  curr_time = Timestamp::now().timeval();

  EtherAddresses remove_list;

  for (InitReqIter i = _initiated_rreq_map.begin(); i.live(); i++) {

    InitiatedReq &ir = i.value();
    assert(ir._target == i.key());

    // we could find out a route by some other means than a direct
    // RREP.  if this is the case, stop issuing requests.
    EtherAddresses route;
    uint32_t metric_of_route;

    _routing_maintenance->query_route( ir._source, ir._target, route, &metric_of_route);
    metric_of_route = _routing_maintenance->get_route_metric(route);

    if(_debug == BrnLogger::DEBUG) {
      BRN_DEBUG(" BUGTRACK: * Metric for route is %d\n * Route is", metric_of_route);

      for (int j=0; j < route.size(); j++) {
        BRN_DEBUG(" - %d  %s", j, route[j].unparse().c_str());
      }
    }

    if ( ( route.size() > 1 )  && ( metric_of_route != 0 ) ) { // route available
      remove_list.push_back(ir._target);
      continue;
    } else {
      if (diff_in_ms(curr_time, ir._time_last_issued) > ir._backoff_interval) {
        if ( (_max_retries == -1 ) || (_max_retries >= ir._times_issued) ) {

          BRN_DEBUG(" * time to issue new request for host %s", ir._target.unparse().c_str());

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

          issue_rreq(ir._target, ir._target_ip, ir._source, ir._source_ip, ir._ttl);
        } else {
          BRN_DEBUG(" * max retries for request for host %s is reached. Remove request.", ir._target.unparse().c_str());
          remove_list.push_back(ir._target);
        }
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
BRN2RouteQuerier::check_blacklist(EtherAddress ether)
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
BRN2RouteQuerier::set_blacklist(EtherAddress ether, int s)
{

  BRN_DEBUG(" set blacklist: %s %d", ether.unparse().c_str(), s);
  BRN_DEBUG(" set blacklist: %d", check_blacklist(ether));

  _blacklist.remove(ether);
  if (s != BRN_DSR_BLACKLIST_NOENTRY) {
    BlacklistEntry e;
    e._time_updated = Timestamp::now().timeval();
    e._status = s;
    _blacklist.insert(ether, e);
  }

  BRN_DEBUG(" set blacklist: %d", check_blacklist(ether));
}

unsigned long
BRN2RouteQuerier::diff_in_ms(timeval t1, timeval t2)
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

bool
BRN2RouteQuerier::metric_preferable(uint32_t a, uint32_t b)
{
  if (!_metric)
    return (a < b); // fallback to minimum hop-count

  return (a < b);
}

uint32_t
BRN2RouteQuerier::route_metric(BRN2RouteQuerierRoute r)
{
  if (r.size() < 2) {
    BRN_WARN(" route_metric: route is too short, less than two nodes????");
    return BRN_DSR_INVALID_ROUTE_METRIC;
  }

  uint32_t sum_metric = 0;

  // the metric in r[i+1] represents the link between r[i] and r[i+1],
  // so we start at 1

  for (int i = 1; i < r.size(); i++) {
    if (r[i]._metric == BRN_DSR_INVALID_HOP_METRIC) return BRN_DSR_INVALID_ROUTE_METRIC;
    sum_metric += r[i]._metric;
  }

  return sum_metric;

#if 0
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
#endif
}

void
BRN2RouteQuerier::add_route_to_link_table(const BRN2RouteQuerierRoute &route, int dsr_element, int end_index)
{
  Vector<EtherAddress> ea_route;

  int route_size = route.size() - 1;
  if ( end_index != -1 ) route_size = end_index;

  for (int i=0; i < route_size; i++) {
    EtherAddress ether1 = route[i].ether();
    EtherAddress ether2 = route[i+1].ether();

    if (ether1 == ether2) {
      if ( i == route.size() - 2 ) ea_route.push_back(ether1);
      continue;
    }

    ea_route.push_back(ether1);

    uint16_t metric;

    switch ( dsr_element ) {
      case DSR_ELEMENT_REQ_FORWARDER: {
        metric = route[i]._metric;     //metric starts with no offset
        break;
      }
      case DSR_ELEMENT_REP_FORWARDER: {
        if (_me->isIdentical(&ether1)) // learn only from route prefix; suffix is not yet set
          break;

        metric = route[i+1]._metric;   //metric starts with offset 1
        //TODO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! remove this shit
        if (metric == 0) metric = 1;
        break;
      }
      case DSR_ELEMENT_SRC_FORWARDER: {
        if (_me->isIdentical(&ether2)) // learn only from route prefix; suffix will be set along the way
          return;

        metric = route[i+1]._metric;   //metric starts with offset 1
        break;
      }
    }

    IPAddress ip1 = route[i].ip();
    IPAddress ip2 = route[i+1].ip();

    //don't learn links from or to me from someone else
    if ( !(_me->isIdentical(&ether1) || _me->isIdentical(&ether2)) ) {
      bool ret = _link_table->update_both_links(ether1, ip1, ether2, ip2, 0, 0, metric);

      if (ret) {
        BRN_DEBUG(" _link_table->update_link %s (%s) %s (%s) %d",
          route[i].ether().unparse().c_str(), route[i].ip().unparse().c_str(),
          route[i+1].ether().unparse().c_str(), route[i+1].ip().unparse().c_str(), metric);
      }
    }
  }

  if ( dsr_element == DSR_ELEMENT_REP_FORWARDER ) {
    uint32_t rmetric = route_metric(route);
    EtherAddress dst = route[0].ether();
    EtherAddress src = route[route.size()-1].ether();

    //if ( !(_me->isIdentical(&dst) || _me->isIdentical(&src)) ) {
      //TODO: check, whether we should restrict route updates (only src/dst or something else
      _routing_maintenance->update_route(src, dst, ea_route, rmetric);
    //}
  }

  ea_route.clear();

  BRN_DEBUG(" * My Linktable: \n%s", _link_table->print_links().c_str());

}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

String
BRN2RouteQuerier::print_stats()
{
  StringAccum sa;

  uint32_t open_requests = 0;
  uint32_t requested_routes = 0;

  for (SBMapIter i = _sendbuffer_map.begin(); i.live(); i++) {
    SBSourceMap &src_map = i.value();
    for (SBSourceMapIter x = src_map.begin(); x.live(); x++) {
      SendBuffer &sb = x.value();

      requested_routes++;
      if ( sb.size() != 0 ) open_requests++;
    }
  }

  sa << "<dsrroutequerierstats node=\"" << BRN_NODE_NAME << "\" expired_packets=\"" << (uint32_t)_expired_packets;
  sa << "\" request=\"" << (uint32_t)_requests << "\" open_requests=\"" << open_requests << "\" next_request_id=\"";
  sa << (_rreq_id+1) << "\" requested_routes=\"" << requested_routes << "\" >\n";

  for (SBMapIter i = _sendbuffer_map.begin(); i.live(); i++) {

    SBSourceMap &src_map = i.value();
    EtherAddress dst = i.key();

    for (SBSourceMapIter x = src_map.begin(); x.live(); x++) {

      SendBuffer &sb = x.value();
      EtherAddress src = x.key();

      sa << "\t<request src=\"" << src.unparse() << "\" dst=\"" << dst.unparse() << "\" packets=\"" << (uint32_t)sb.size() << "\" />\n";
    }
  }

  sa << "</dsrroutequerierstats>\n";

  return sa.take_string();
}

enum { H_FIXED_ROUTE, H_FIXED_ROUTE_CLEAR, H_FLUSH_SB, H_STATS, H_MAX_RETRIES};

static String
read_handler(Element *e, void * vparam)
{
  BRN2RouteQuerier *rq = (BRN2RouteQuerier *)e;

  switch ((intptr_t)vparam) {
    case H_FIXED_ROUTE: {
      String ret;
      for (BRN2RouteQuerier::RouteMap::const_iterator iter = rq->fixed_routes.begin();
          iter.live(); ++iter) {
        const BRN2RouteQuerier::EtherPair& pair = iter.key();
        ret += pair.first.unparse() + " -> " + pair.second.unparse() + ":\n";

        const EtherAddresses& route = iter.value();
        for (int i = 0; i < route.size(); i++) {
          const EtherAddress& addr = route.at(i);
          ret += "    " + addr.unparse() + "\n";
        }
          ret += "\n";
      }
      return ret;
    }
    case H_STATS: {
      return rq->print_stats();
    }
    case H_MAX_RETRIES: {
      return "" + String(rq->_max_retries) + "\n";
    }
  }

  return String("n/a\n");
}

static int 
write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BRN2RouteQuerier *rq = (BRN2RouteQuerier *)e;
  String s = cp_uncomment(in_s);

  switch ((intptr_t)vparam) {
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
        click_chatter("add ether %s to route\n", m.unparse().c_str());
        routeForward.push_back(m);
        routeReverse.insert(routeReverse.begin(), m);
      }

      int size = routeForward.size();
      if (size < 2)
        return errh->error("fixed_route must contain at least 2 entries.");

      BRN2RouteQuerier::EtherPair keyForward(routeForward[0], routeForward[size-1]);
      rq->fixed_routes.insert(keyForward, routeForward);

      BRN2RouteQuerier::EtherPair keyReverse(routeReverse[0], routeReverse[size-1]);
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
    case H_MAX_RETRIES: {
      int32_t max_retries;

      if (!cp_integer(s, &max_retries))
        return errh->error("flush_sb parameter must be a boolean.");

      rq->_max_retries = max_retries;

      break;
    }
  }
  return 0;
}

void
BRN2RouteQuerier::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_handler, (void*) H_STATS);
  add_read_handler("fixed_route", read_handler, (void*) H_FIXED_ROUTE);
  add_read_handler("max_retries", read_handler, (void*) H_MAX_RETRIES);

  add_write_handler("fixed_route", write_handler, (void*) H_FIXED_ROUTE);
  add_write_handler("fixed_route_clear", write_handler, (void*) H_FIXED_ROUTE_CLEAR);
  add_write_handler("flush_sb", write_handler, (void*) H_FLUSH_SB);
  add_write_handler("max_retries", write_handler, (void*) H_MAX_RETRIES);
}


ELEMENT_REQUIRES(LinkTable)
EXPORT_ELEMENT(BRN2RouteQuerier)

#include <click/bighashmap.cc>
#include <click/vector.cc>
template class HashMap<ForwardedReqKey, ForwardedReqVal>;
template class HashMap<EtherAddress, BRN2RouteQuerier::InitiatedReq>;
template class Vector<BRN2RouteQuerier::BufferedPacket>;
template class HashMap<EtherAddress, BRN2RouteQuerier::SendBuffer>;
template class HashMap<EtherAddress, BRN2RouteQuerier::BlacklistEntry>;
template class HashMap<BRN2RouteQuerier::EtherPair,EtherAddresses>;

CLICK_ENDDECLS
