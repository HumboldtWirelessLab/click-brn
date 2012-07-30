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
 * reqforwarder.{cc,hh} -- forwards dsr route request packets
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/brn2.h"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "brn2_dsrprotocol.hh"
#include "brn2_reqforwarder.hh"

CLICK_DECLS

BRN2RequestForwarder::BRN2RequestForwarder()
  :_me(),
  _link_table(),
  _dsr_decap(),
  _dsr_encap(),
  _route_querier(),
  _min_metric_rreq_fwd(BRN_DSR_DEFAULT_MIN_METRIC_RREQ_FWD),
  _max_age(DEFAULT_REQUEST_MAX_AGE_MS),
  _retransmission_timer(static_rreq_retransmit_timer_hook,this),
  _sendbuffer_timer(this),
  _enable_last_hop_optimization(false),
  _enable_full_route_optimization(false),
  _enable_delay_queue(true),
  _stats_receive_better_route(0),
  _stats_avoid_bad_route_forward(0),
  _stats_opt_route(0),
  _stats_del_passive_ack_retransmissioninfo(0),
  _stats_del_passive_ack_reason_full_neighbours(0),
  _stats_del_passive_ack_reason_full_retries(0),
  _stats_del_passive_ack_inserts(0),
  _stats_del_passive_ack_reinserts(0),
  _passive_ack_retries(0),
  _passive_ack_interval(0),
  _passive_ack_force_retries(false)
{
  BRNElement::init();
}

BRN2RequestForwarder::~BRN2RequestForwarder()
{
}

int
BRN2RequestForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      "DSRDECAP", cpkP+cpkM, cpElement, &_dsr_decap,
      "DSRENCAP", cpkP+cpkM, cpElement, &_dsr_encap,
      "ROUTEQUERIER", cpkP+cpkM, cpElement, &_route_querier,
      "MINMETRIC", cpkP+cpkM, cpInteger, &_min_metric_rreq_fwd,
      "LAST_HOP_OPT", cpkP, cpBool, &_enable_last_hop_optimization,
      "FULL_ROUTE_OPT", cpkP, cpBool, &_enable_full_route_optimization,
      "ENABLE_DELAY_QUEUE", cpkP, cpBool, &_enable_delay_queue,
      "PASSIVE_ACK_RETRIES", cpkP, cpInteger, &_passive_ack_retries,
      "PASSIVE_ACK_INTERVAL", cpkP, cpInteger, &_passive_ack_interval,
      "FORCE_PASSIVE_ACK_RETRIES", cpkP, cpBool, &_passive_ack_force_retries,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_dsr_decap || !_dsr_decap->cast("BRN2DSRDecap")) 
    return errh->error("DSRDecap not specified");

  if (!_dsr_encap || !_dsr_encap->cast("BRN2DSREncap"))
    return errh->error("DSREncap not specified");

  if (!_route_querier || !_route_querier->cast("BRN2RouteQuerier"))
    return errh->error("RouteQuerier not specified");

  return 0;
}

int
BRN2RequestForwarder::initialize(ErrorHandler *)
{
  click_srandom(_me->getMasterAddress()->hashcode());

  _sendbuffer_timer.initialize(this);

  _last_neighbor_update = Timestamp::now();
  _neighbors.clear();

  _retransmission_timer.initialize(this);

  return 0;
}

void
BRN2RequestForwarder::uninitialize()
{
  //cleanup
}

int
BRN2RequestForwarder::findOwnIdentity(const BRN2RouteQuerierRoute &r)
{
  for (int i=0; i<r.size(); i++) {
    EtherAddress ea = EtherAddress((r[i].ether()).data());
    if ( _me->isIdentical(&ea) )
      return i;
  }
  return -1;
}

int
BRN2RequestForwarder::findInRoute(const BRN2RouteQuerierRoute &r, EtherAddress *node)
{
  for (int i=0; i<r.size(); i++) {
    EtherAddress ea = EtherAddress((r[i].ether()).data());
    if ( ea == *node ) return i;
  }
  return -1;
}


/* process an incoming route request packet */
void
BRN2RequestForwarder::push(int, Packet *p_in)
{
  uint8_t devicenumber = BRNPacketAnno::devicenumber_anno(p_in);
  BRN2Device *indev = _me->getDeviceByNumber(devicenumber);
  const EtherAddress *device_addr = indev->getEtherAddress(); //ether addr of the interface the packet is coming from

  const click_ether *ether = (const click_ether *)p_in->ether_header();
  click_brn *brn = (click_brn *)p_in->data();
  // rreq received from this node (last hop)
  EtherAddress prev_node(ether->ether_shost);

  BRN_DEBUG("* receiving dsr_rreq packet (packet_anno %s)",
            _me->getDeviceByNumber(devicenumber)->getDeviceName().c_str());

  const click_brn_dsr *brn_dsr = (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

  assert(brn_dsr->dsr_type == BRN_DSR_RREQ);

  EtherAddress src_addr(brn_dsr->dsr_src.data); // src of the rreq
  EtherAddress dst_addr(brn_dsr->dsr_dst.data); // final dst of the rreq

  // fetch ip address of clients
  IPAddress src_ip_addr(brn_dsr->dsr_ip_src);
  IPAddress dst_ip_addr(brn_dsr->dsr_ip_dst);

  BRN_DEBUG(" got route request for destination %s from %s with #ID %d",
      dst_addr.unparse().c_str(), prev_node.unparse().c_str(), ntohs(brn_dsr->dsr_id));

  // Last hop metric: metric from last node (sender of this packet) to me
  uint16_t last_hop_metric = _link_table->get_link_metric(prev_node, *device_addr);

  BRN_DEBUG("Last_hop_metric: %d",last_hop_metric);
  //BRN_DEBUG(_link_table->print_links().c_str());
  //BRN_DEBUG(" * my (%s) metric for last hop (%s) is) %d", my_rec_addr->unparse().c_str(),
  //                                                       last_node_addr.unparse().c_str(), metric);

  // get the so far estimated route from rreq
  BRN2RouteQuerierRoute request_route;
  // at this point the route is constructed from the src node to the last hop
  // SRC, HOP1, ..., LAST HOP
  // DSRDecap has linktable to get last_hop_metric
  uint16_t route_metric = _dsr_decap->extract_request_route(p_in, request_route, &prev_node, last_hop_metric);

  //Learn from route request
  BRN_DEBUG("* learned from RREQ ...");

  if (BRN_DEBUG_LEVEL_DEBUG) {
    for (int j = 0; j < request_route.size(); j++)
      BRN_DEBUG(" RREQ - %d   %s (%d)", j, request_route[j].ether().unparse().c_str(), request_route[j]._metric);
  }

    // refresh own link table
  _route_querier->add_route_to_link_table(request_route, DSR_ELEMENT_REQ_FORWARDER, -1);

  //IAPP stuff
  /*
  // sanity check - does a client reassociate?
  // TODO the information here is shit, leads to false roaming decisions

  EtherAddress first_ether = request_route[1].ether();

  if (true == _client_assoc_lst->is_associated(src_addr)
      && 1 <= request_route.size()
      && false == _me->isIdentical(&first_ether)) {

    // Remove the assocation from assoc_list
    _iapp->sta_roamed(src_addr, first_ether, *_me->getMyWirelessAddress());
  }
  */

  if ( (_me->isIdentical(&src_addr)) ||
       (_link_table->is_associated(src_addr)) ||
       (findOwnIdentity(request_route) != -1) ) {
    BRN_DEBUG("* I (=%s) sourced this RREQ or already listed; ignore., #ID %d", device_addr->unparse().c_str(),
                                                                                ntohs(brn_dsr->dsr_id));

    if ( _passive_ack_interval != 0 ) {
      BRN_DEBUG("Check passive");
      check_passive_ack(&prev_node, &src_addr, ntohs(brn_dsr->dsr_id));
    } else {
      BRN_DEBUG("Don't check passive interval: %d",_passive_ack_interval);
    }
    p_in->kill();
    return;
  }

  if ( !(_me->isIdentical(&dst_addr) || _link_table->is_associated(dst_addr)) ) {
    if ( brn->ttl == 1) {
      BRN_DEBUG(" * time to live expired; killing packet\n");
      p_in->kill();
      _stats_avoid_bad_route_forward++;
      return;
      } // ttl is decremented in forward_rreq
  }

  if ( _enable_last_hop_optimization || ( _passive_ack_interval > 0 ) ) {
    Timestamp now = Timestamp::now();

    if ( _link_table && ((_neighbors.size() == 0) ||
        ((now - _last_neighbor_update).msecval() > 1000)) ) {
      _neighbors.clear();
      _link_table->get_local_neighbors(_neighbors);
      _last_neighbor_update = now;
    }
  }


  //Route improvements
  EtherAddress *detour_nb = NULL;
  int detour_metric_last_nb;

  /* Route Optimization */
  if ( _enable_last_hop_optimization && (brn->ttl > 1) ) {
    /* Last hop optimization */
    BRN_DEBUG("* RREQ: Try to optimize route from last hop to me.");

    // metric between last hop and me: last_hop_metric

    // search in my 1-hop neighborhood for a node which is well-connected with prev_node
    if ( _link_table && (_neighbors.size() > 0) ) {
      int best_metric_last_nb = BRN_DSR_INVALID_HOP_METRIC;
      int best_metric_nb_me = BRN_DSR_INVALID_HOP_METRIC;
      int best_detour_metric = BRN_DSR_INVALID_HOP_METRIC;
      EtherAddress best_nb;

      for( int n_i = 0; n_i < _neighbors.size(); n_i++) {
        // calc metric between this neighbor and last hop (= prev_node)
        int metric_last_nb = _link_table->get_link_metric(prev_node, _neighbors[n_i]);

        // calc metric between this neighbor and myself
        int metric_nb_me = _link_table->get_link_metric(_neighbors[n_i], *device_addr);

        int detour_metric = metric_last_nb + metric_nb_me;
        if ( (detour_metric < last_hop_metric) && (detour_metric < best_detour_metric) ) {
            // save this nb
          best_detour_metric = detour_metric;
          best_metric_last_nb = metric_last_nb;
          best_metric_nb_me = metric_nb_me;
          best_nb = _neighbors[n_i];
        }
      }

      if (best_detour_metric < last_hop_metric && findInRoute(request_route, &best_nb) == -1 ) {
        BRN_DEBUG("* RREQ: Insert detour over %s. with metric %d instead of %d",
                  best_nb.unparse().c_str(), best_detour_metric, last_hop_metric);
        //Change metric to last hop since this is not between the node and me, instead it is between last hop
        //and the new last hop
        request_route[request_route.size()-1]._metric = best_metric_last_nb;
          // lets make a detour over best_nb
        request_route.push_back(BRN2RouteQuerierHop(best_nb, best_metric_nb_me));
        detour_nb = &best_nb;
        detour_metric_last_nb = best_metric_last_nb;

        //fix route metric (change due rerouting)
        route_metric = route_metric - last_hop_metric + best_detour_metric;

        last_hop_metric = best_metric_nb_me;

        _stats_opt_route++;
      }
    }

    /* Last hop optimization */
    BRN_DEBUG("* RREQ: Finishing last hop optimization.");
  } else if ( _enable_full_route_optimization ) {
    /* Full route optimazation */
    BRN_DEBUG("* RREQ: Try to optimize route from src to me.");
  }

  //Forward routerequest only if it fits requirements
  BRN_DEBUG(" * Last_hop_metric %d, #ID %d", last_hop_metric, ntohs(brn_dsr->dsr_id));

  if ( last_hop_metric >= _min_metric_rreq_fwd ) {
    BRN_DEBUG(" Last_hop_metric %d is inferior as min_metric_rreq_fwd %d, #ID %d",
      last_hop_metric, _min_metric_rreq_fwd, ntohs(brn_dsr->dsr_id));
    BRN_DEBUG(" Kill Routerequest from junk link, #ID %d", ntohs(brn_dsr->dsr_id));
    p_in->kill();
    _stats_avoid_bad_route_forward++;
    return;
  }

  BRN_DEBUG("_track_route_map: %d\n", _track_route_map.size());
  RouteRequestInfo *rreqi = NULL;

  if ( _track_route_map.size() > 0 ) {
    /* Check for already forwarded request */
    rreqi = _track_route_map.find(src_addr);
  }

  if ( rreqi == NULL ) {
    rreqi = new RouteRequestInfo(&src_addr, _max_age);
    _track_route_map.insert(src_addr, rreqi);
  }

  Timestamp now = Timestamp::now();
  BRN_DEBUG("compare routes. Is a better one ? %d < %d ?", route_metric,
                                                           rreqi->get_current_metric(ntohs(brn_dsr->dsr_id), &now));

  if ( _route_querier->metric_preferable(route_metric, rreqi->get_current_metric(ntohs(brn_dsr->dsr_id), &now)) ) {
    rreqi->set_metric(ntohs(brn_dsr->dsr_id), route_metric, &now, (uint8_t)((detour_nb==NULL)?0:1));
    _stats_receive_better_route++;
  } else {
      //already forwarded with better metric
    BRN_DEBUG(" Seen Routerequest with better metric before; Drop packet! %d <= %d, #ID %d",
              rreqi->get_current_metric(ntohs(brn_dsr->dsr_id), &now), route_metric, ntohs(brn_dsr->dsr_id));
    _stats_avoid_bad_route_forward++;
    p_in->kill(); // kill the original RREQ
    return;
  }

  // Check whether we have reached the final destination (the meshnode itself or an associated client station).
  // Continue the construction of the route up to the final destination
  // SRC, HOP1, ..., LAST HOP, THIS1, THIS2, DST
  if ( _me->isIdentical(&dst_addr) || ( _link_table->is_associated(dst_addr)) ) {

    BRN_DEBUG("* RREQ: metric to prev node (%s) is %d.", prev_node.unparse().c_str(), last_hop_metric);

    // the requested client is associated with this node
    if (_link_table->is_associated(dst_addr)) {

      BRN_DEBUG("er soll zu mir gehoeren");
      BRN_DEBUG(" * Linktable: %s", _link_table->print_links().c_str());

      //this should give back only one etheraddress if the client is associated
      const EtherAddress *my_wlan_addr = _link_table->get_neighbor(dst_addr);

      BRN_DEBUG(" * addr of current node added to route(2): %s.", my_wlan_addr->unparse().c_str());

      //Is Client associated using the device where this  routerequest is received
      if ( *my_wlan_addr != *device_addr ) {
        //if received device is different to associated device, then add assoc device to route (memory hop)
        request_route.push_back(BRN2RouteQuerierHop(*my_wlan_addr, BRN_LT_MEMORY_MEDIUM_METRIC));//link to received device to assoc dev
      }

      //put myself into request route; use the metric to reach the client
      request_route.push_back(BRN2RouteQuerierHop(*device_addr, BRN_LT_STATION_METRIC));//link to associated clients
                                                                                        //is always 100

      //TODO: update route-metric

    } else if (_me->isIdentical(&dst_addr)) { //an internal node is the receiver
      BRN_DEBUG("* an internal node is the receiver; add myself to route.");

      // put my address into route
      request_route.push_back(BRN2RouteQuerierHop(*device_addr, 0)); // metric field is not used
    }

    BRN_DEBUG("* addr of assoc client/this node added to route.");
    // put the address of the client into route
    request_route.push_back(BRN2RouteQuerierHop(dst_addr, dst_ip_addr, 0)); // metric not used

    // this RREQ is for me, so generate a reply.
    BRN2RouteQuerierRoute reply_route;

    // reply route is simply the inverted request route
    reverse_route(request_route, reply_route);

    uint16_t route_met = 0;
    for ( int j = 1; j < ( reply_route.size() - 1 ); j++)
      route_met += reply_route[j]._metric;

    BRN_DEBUG("* Generating route reply with source route");
    for (int j = 0; j < reply_route.size(); j++) {
      BRN_DEBUG(" - %d   %s (%d)",
                j, reply_route[j].ether().unparse().c_str(),
                                        reply_route[j]._metric);
    }

    issue_rrep(dst_addr, dst_ip_addr, src_addr, src_ip_addr, reply_route, ntohs(brn_dsr->dsr_id));
    p_in->kill(); // kill the original RREQ

  } else { // this RREQ is not for me.  decide whether to forward it.
    // check to see if we've seen this request lately, or if this
    // one is better
    ForwardedReqKey frk(src_addr, dst_addr, ntohs(brn_dsr->dsr_id));
    ForwardedReqVal *old_frv = _route_querier->_forwarded_rreq_map.findp(frk);

    /* add myself to route to get metric from src to me */
    request_route.push_back(BRN2RouteQuerierHop(*device_addr, dst_ip_addr, 0)); // metric not used

    unsigned short this_metric = _route_querier->route_metric(request_route);

    BRN_DEBUG("* RREQ: this_metric is %d last_hop_metric is %d",route_metric, last_hop_metric);

    if (old_frv) {
      if ( BRN_DEBUG_LEVEL_DEBUG ) {
        BRN_DEBUG("* already forwarded this route request %d (%d, %d)",
                  ntohs(brn_dsr->dsr_id), route_metric, old_frv->best_metric);

        if (_route_querier->metric_preferable(route_metric, old_frv->best_metric)) {
          BRN_DEBUG(" * but this one is better");
        } else {
          BRN_DEBUG("* and this one's not as good ( %d vs. %d )", route_metric, old_frv->best_metric);
        }
      }
    }

    if (old_frv && ! _route_querier->metric_preferable(route_metric, old_frv->best_metric)) {
      BRN_DEBUG("* already forwarded this route request. #ID %d", ntohs(brn_dsr->dsr_id));
      p_in->kill();
      return;
    } else { // not yet seen
      BRN_DEBUG(" * RREQ not yet seen.");
      // we have not seen this request before, or this one is
      // 'better'; before we do the actual forward, check
      // blacklist for the node from which we are receiving this.
      ForwardedReqVal new_frv; // new entry for _forwarded_rreq_map

      struct timeval current_time;
      current_time = Timestamp::now().timeval();
      new_frv._time_forwarded = current_time;

      EtherAddress last_forwarder = request_route[request_route.size()-2].ether();

      BRN_DEBUG("* last_forwarder is %s", last_forwarder.unparse().c_str());

      if (old_frv ) {
        _stats_receive_better_route++;

        if (old_frv->p) {
          // if we're here, then we've already forwarded this
          // request, but this one is better and we want to
          // forward it.  however, we've got a pending
          // unidirectionality test for this RREQ.

          // what we should do is maintain a list of packet *'s
          // that we've issued tests for.

          // XXX instead, we just give up on the potentially
          // asymmetric link.  whether or not the test comes back,
          // things should be ok.  nonetheless this is incorrect
          // behavior.

          /* RobAt: optimization: add Buffered_packet to ForwardedReqVal
            - if better route is received, change the packet in bufferd packet
              and kill the old one */
          old_frv->p->kill();
          old_frv->p = NULL;
        }
      }

      BRN_DEBUG("* forwarding this RREQ %s", indev->getDeviceName().c_str());

      new_frv.p = NULL;
      new_frv.best_metric = this_metric;
      _route_querier->_forwarded_rreq_map.insert(frk, new_frv);
      forward_rreq(p_in, detour_nb, detour_metric_last_nb, rreqi, ntohs(brn_dsr->dsr_id));
    }
  }
}

/*
 * rebroadcast a route request.  we've already checked that we haven't
 * seen this RREQ lately, and added the info to the forwarded_rreq_map.
 *
 */
void
BRN2RequestForwarder::forward_rreq(Packet *p_in, EtherAddress *detour_nb, int detour_metric_last_nb,
                                   RouteRequestInfo *rri, uint16_t rreq_id)
{
  uint8_t devicenumber = BRNPacketAnno::devicenumber_anno(p_in);
  BRN2Device *indev;

  // add my address to the end of the packet
  WritablePacket *p_u = p_in->uniqueify();

  int addHops = 1;
  if (detour_nb) {
    BRN_DEBUG("* Adding detour node %s.", detour_nb->unparse().c_str());
    addHops++;
  }

  WritablePacket *p = DSRProtocol::extend_hops(p_u,addHops);  //add space for one additional hop

  BRN_DEBUG("Headersize: %d brn+dsr: %d",p->length(),sizeof(click_brn) + sizeof(click_brn_dsr));

  click_brn *brn = (click_brn *)p->data();
  click_brn_dsr *dsr_rreq = (click_brn_dsr *)(p->data() + sizeof(click_brn));

  int hop_count = dsr_rreq->dsr_hop_count;

  BRN_DEBUG("* BRN2RequestForwarder::forward_rreq with #ID %d", ntohs(dsr_rreq->dsr_id));

  if (hop_count >= BRN_DSR_MAX_HOP_COUNT) {
    BRN_DEBUG("* Maximum hop count reached in rreq; drop packet.");
    return;
  }

  // new hop is the address of the last node
  //EtherAddress hop(ether->ether_shost);
  click_ether *ether = (click_ether *)p->ether_header();

  // rreq received from this node ...
  EtherAddress prev_node(ether->ether_shost);

  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(dsr_rreq);//RobAt:DSR

  memcpy(dsr_hops[hop_count].hw.data, (uint8_t *)prev_node.data(), 6 * sizeof(uint8_t));  //TODO: extend for new entry

  if (detour_nb) {
    memcpy(dsr_hops[hop_count+1].hw.data, (uint8_t *)detour_nb->data(), 6 * sizeof(uint8_t));  //add detour node
  }

  //rreq is a broadcast; use the ether address associated with packet's device
  indev = _me->getDeviceByNumber(devicenumber);
  const EtherAddress *me = indev->getEtherAddress(); // ethernet addr of the interface the packet is coming from

  if (me) {
    if (detour_nb) {
      dsr_hops[hop_count].metric = htons(detour_metric_last_nb);
      int metric = _link_table->get_link_metric(*detour_nb, *me);
      dsr_hops[hop_count+1].metric = htons(metric);
    } else { // default behavior
      // set the metric no my previous node
      int metric = _link_table->get_link_metric(prev_node, *me);
      BRN_DEBUG("* append prev node (%s) to rreq with metric %d.", prev_node.unparse().c_str(), metric);
      dsr_hops[hop_count].metric = htons(metric);
    }
  } else {
    BRN_DEBUG("* device unknown: %s", indev->getDeviceName().c_str());
  }

  // update hop count
  dsr_rreq->dsr_hop_count++;
  // reduce ttl
  brn->ttl--;

  if (detour_nb) {
    dsr_rreq->dsr_hop_count++;
    brn->ttl--;
  }

  // set source and destination anno
  BRN_DEBUG("New SRC-Ether is: %s",indev->getEtherAddress()->unparse().c_str());

  //TODO: ANNO or header ??
  BRNPacketAnno::set_src_ether_anno(p,EtherAddress(indev->getEtherAddress()->data()));
  BRNPacketAnno::set_dst_ether_anno(p,EtherAddress((const unsigned char *)"\xff\xff\xff\xff\xff\xff"));
  BRNPacketAnno::set_ethertype_anno(p,ETHERTYPE_BRN);

 // click_ether *annotated_ether = (click_ether *)p->ether_header();
  memcpy(ether->ether_shost,indev->getEtherAddress()->data() , 6);
  memcpy(ether->ether_dhost, EtherAddress((const unsigned char *)"\xff\xff\xff\xff\xff\xff").data(), 6);
  ether->ether_type = htons(ETHERTYPE_BRN);

  // copy device anno
  BRNPacketAnno::set_devicenumber_anno(p,devicenumber);
  BRN_DEBUG(" * current dev_anno %s.", indev->getDeviceName().c_str());

  //retransmission only make sense if we have more neighbors than last hop (1 node)
  BRN_DEBUG("RetransInfo: %d %d",_passive_ack_interval,_neighbors.size() );
  if ( (_passive_ack_interval != 0) && (_neighbors.size() > 1) ) {
    RReqRetransmitInfo *rreq_retr_i;
    bool just_update = true;

    int old_index = retransmission_index(&(rri->_src), rreq_id);

    if ( old_index == -1 ) { //if this request id doesn't exists
      rreq_retr_i = new RReqRetransmitInfo(p->clone(), &(rri->_src), rreq_id);
      _rreq_retransmit_list.push_back(rreq_retr_i);
      just_update = false;
      _stats_del_passive_ack_inserts++;
    } else {
      rreq_retr_i = _rreq_retransmit_list[old_index];
      BRN_DEBUG("Alreday schedule this rreq %d", old_index);
      if ( rreq_retr_i->_p == NULL ) {
        BRN_DEBUG("WTF! No packet");
      } else {
        rreq_retr_i->_p->kill();
      }
      _stats_del_passive_ack_reinserts++;
      rreq_retr_i->_p = p->clone();
    }

    rri->reset_passive_ack(rreq_id,
                           (_neighbors.size()>PASSIVE_ACK_MAX_NEIGHBOURS)?PASSIVE_ACK_MAX_NEIGHBOURS:_neighbors.size(),
                           _passive_ack_retries);

    int32_t ion = index_of_neighbor(&prev_node);
    if ( ion != -1 )
      rri->received_neighbour(rreq_id, (uint16_t)ion);

    BRN_DEBUG("RetransmitQueue - Size: %d", _rreq_retransmit_list.size());
    //only start timer if we didn't start it yet (first retransmission in the queue)
    if ( ((rri->has_neighbours_left(rreq_id)) || (_passive_ack_force_retries)) &&
          ( _rreq_retransmit_list.size() == 1 ) && (!just_update)) {
      BRN_DEBUG("Start Retransmit - Size: %d", _rreq_retransmit_list.size());
      _retransmission_timer.schedule_after_msec(_passive_ack_interval);
    }
  }

  if ( _enable_delay_queue ) {
    // Buffering + Jitter
    unsigned int j = (unsigned int ) ( ( click_random() % ( JITTER ) ) );
    if ( j <= 1 ) {
      output(0).push(p);
    } else {
      _packet_queue.addPacket(p, j);

      j = _packet_queue.getTimeToNext();

      _sendbuffer_timer.schedule_after_msec(j);
    }
  } else {
    output(0).push(p);
  }
}

/* method generates and sends a dsr route reply */
void
BRN2RequestForwarder::issue_rrep(EtherAddress src, IPAddress src_ip, EtherAddress dst,
                                 IPAddress dst_ip, const BRN2RouteQuerierRoute &reply_route, uint16_t rreq_id)
{
  BRN_DEBUG("* issue_rrep: ... #ID %d", rreq_id);
  Packet *brn_p;

  //prepend dsr header
  Packet *rrep_p = _dsr_encap->create_rrep(dst, dst_ip, src, src_ip, reply_route, rreq_id);
  //prepend brn header
  assert(rrep_p);
  brn_p = BRNProtocol::add_brn_header(rrep_p, BRN_PORT_DSR, BRN_PORT_DSR, 255, BRNPacketAnno::tos_anno(rrep_p));

  //output route reply packet
  output(1).push(brn_p);
}

//-------------------------------------------------------------------------------
// HELPER METHODS FOR PASSIVE ACK
//-------------------------------------------------------------------------------

//check whether 
void
BRN2RequestForwarder::check_passive_ack(EtherAddress *last_node_addr, EtherAddress *src, uint16_t rreq_id)
{
  int index = retransmission_index(src, rreq_id);

  BRN_DEBUG("Check for passive ack: LN %s SRC: %s REQID: %d INDEX: %d Size: %d",last_node_addr->unparse().c_str(),
                                                                                src->unparse().c_str(),rreq_id,
                                                                                index, _rreq_retransmit_list.size());

  if ( index == -1 ) return;

  RouteRequestInfo *rri = _track_route_map.find(*src);
  int32_t ion = index_of_neighbor(last_node_addr);

  BRN_DEBUG("Retransmission-Index: %d Index of Neighbour: %d", index, ion);

  if ( ion == -1 ) return;

  RReqRetransmitInfo *rreq_retr_i = _rreq_retransmit_list[index];

  rri->received_neighbour(rreq_id, (uint16_t)ion);

  if ( (!rri->has_neighbours_left(rreq_id)) && (!_passive_ack_force_retries) ) {
    BRN_DEBUG("Delete Retransmit rreq");
    rreq_retr_i->_p->kill();
    delete _rreq_retransmit_list[index];
    _rreq_retransmit_list.erase(_rreq_retransmit_list.begin() + index);
    _stats_del_passive_ack_retransmissioninfo++;
    _stats_del_passive_ack_reason_full_neighbours++;
  }

}

int
BRN2RequestForwarder::retransmission_index(EtherAddress *ea, uint16_t rreq_id)
{
  for ( int i = 0; i < _rreq_retransmit_list.size();i++ )
    if ( (rreq_id == _rreq_retransmit_list[i]->_rreq_id) && (_rreq_retransmit_list[i]->_src == *ea) ) return i;

  return -1;
}


//-----------------------------------------------------------------------------
// Timer methods
//-----------------------------------------------------------------------------

void
BRN2RequestForwarder::rreq_retransmit_timer_hook()
{
  BRN_DEBUG("Retransmit - Size: %d", _rreq_retransmit_list.size());
  int send_packets = 0;
  for ( int i = _rreq_retransmit_list.size() - 1; i >= 0; i-- ) {

    Packet *p = NULL;

    RReqRetransmitInfo *rreq_retr_i = _rreq_retransmit_list[i];
    RouteRequestInfo *rri = _track_route_map.find(rreq_retr_i->_src);

    if ( ! rri ) continue;
    if ( ! rri->includes_rreq(rreq_retr_i->_rreq_id) ) {
      rreq_retr_i->_p->kill();
      //now delete rreqretransmitinfo
      delete rreq_retr_i;
      _rreq_retransmit_list.erase(_rreq_retransmit_list.begin() + i);
      _stats_del_passive_ack_retransmissioninfo++;
      continue;
    }

    rri->dec_retries(rreq_retr_i->_rreq_id);
    if ( rri->left_retries(rreq_retr_i->_rreq_id) == 0 ) {
      BRN_DEBUG("Last retry for passive ack. Delete rreq_retr");
      p = rreq_retr_i->_p;
      //delete rreqretransmitinfo
      delete rreq_retr_i;
      _rreq_retransmit_list.erase(_rreq_retransmit_list.begin() + i);
      _stats_del_passive_ack_reason_full_retries++;
      _stats_del_passive_ack_retransmissioninfo++;
    } else {
      p = rreq_retr_i->_p->clone();
    }

    BRN_DEBUG("Retransmit rreq");

    if ( _enable_delay_queue ) {
      // Buffering + Jitter
      unsigned int j = (unsigned int ) ( ( click_random() % ( JITTER ) ) );
      if ( j <= 1 )
        output(0).push(p);
      else {
        send_packets++;
        _packet_queue.addPacket(p, j);
      }
    } else {
        output(0).push(p);
    }
  }

  if ( send_packets > 0 ) {
    unsigned int j = _packet_queue.getTimeToNext();
    _sendbuffer_timer.schedule_after_msec(j);
  }

  if ( _rreq_retransmit_list.size() > 0 ) {
    _retransmission_timer.schedule_after_msec(_passive_ack_interval);
  }
}

void
BRN2RequestForwarder::static_rreq_retransmit_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  BRN2RequestForwarder *rreq_fwd = (BRN2RequestForwarder *)f;
  rreq_fwd->rreq_retransmit_timer_hook();
}

void
BRN2RequestForwarder::run_timer(Timer *)
{
  BRN_DEBUG("* RREQ: Run timer.");
  queue_timer_hook();
}

void
BRN2RequestForwarder::queue_timer_hook()
{
  struct timeval curr_time = Timestamp::now().timeval();

  for ( int i = (_packet_queue.size()-1); i >= 0; i--) {
    if ( _packet_queue.get(i)->diff_time(curr_time) <= 0 )
    {
      Packet *out_packet = _packet_queue.get(i)->_p;
      _packet_queue.del(i);
      output(0).push( out_packet );
      break;
    } else {
      BRN_DEBUG("Time left: %d",_packet_queue.get(i)->diff_time(curr_time));
    }
  }

  int j = _packet_queue.getTimeToNext();

  if ( j >= 0 ) {
    if ( j < MIN_JITTER ) j = MIN_JITTER;

    _sendbuffer_timer.schedule_after_msec(j);
  }
}

//-----------------------------------------------------------------------------
// Helper methods
//-----------------------------------------------------------------------------

void
BRN2RequestForwarder::reverse_route(const BRN2RouteQuerierRoute &in, BRN2RouteQuerierRoute &out)
{
  for(int i=in.size()-1; i>=0; i--)
    out.push_back(in[i]);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {
  H_DSR_STATS, H_TRACK
};

static String
read_param(Element *e, void *thunk)
{
  BRN2RequestForwarder *rreq = (BRN2RequestForwarder *)e;
  StringAccum sa;

  switch ((uintptr_t) thunk)
  {
    case H_DSR_STATS :
    {
      sa << "<requestfwdstats better_route=\"" << rreq->_stats_receive_better_route;
      sa << " avoid_bad_route_forward=\"" << rreq->_stats_avoid_bad_route_forward << "\" opt_route=\"";
      sa << rreq->_stats_opt_route << "\">\n";
      sa << "\t<passive_ack_stats del_passive_ack_retransmissioninfo=\"" <<  rreq->_stats_del_passive_ack_retransmissioninfo;
      sa << "\" full_neighbours=\"" << rreq->_stats_del_passive_ack_reason_full_neighbours << "\" full_retries=\"";
      sa << rreq->_stats_del_passive_ack_reason_full_retries << "\" passive_ack_list_size=\"" << rreq->_rreq_retransmit_list.size();
      sa << "\" inserts=\"" << rreq->_stats_del_passive_ack_inserts << "\" reinserts=\"" << rreq->_stats_del_passive_ack_reinserts << "\" />\n";
      sa << "</requestfwdstats>\n";
      return ( sa.take_string() );
    }
    case H_TRACK :
    {
      return rreq->get_trackroutemap();
    }
  }

  return String();
}

String
BRN2RequestForwarder::get_trackroutemap() {
  StringAccum sa;

  sa << "<routemap id=\"" << BRN_NODE_NAME << "\" trackmapsize=\"" << _track_route_map.size() << "\">\n";

  for (TrackRouteMapIter iter = _track_route_map.begin(); iter.live(); iter++) {
    EtherAddress ea = iter.key();
    RouteRequestInfo *rri = iter.value();

    sa << "\t<requestnode src=\"" << ea.unparse() << "\" >\n";

    for ( int i = 0; i < METRIC_LIST_SIZE; i++) {
      if ( (rri->_id_list[i]) % METRIC_LIST_SIZE == i ) {
        sa << "\t\t<request id=\"" << rri->_id_list[i] << "\" metric=\"" << rri->_metric_list[i];
        sa << "\" last_hop_opt=\"" << (uint32_t)(rri->_last_hop_opt[i]) << "\" left_retries=\"";
        sa << (uint32_t)(rri->_passive_ack_retry_list[i]) << "\" last_retry=\"" << rri->_passive_ack_last_retry_list[i].unparse() << "\" />\n";
      }
    }
    sa << "\t</requestnode>\n";
  }

  sa << "</routemap>\n";

  return sa.take_string();
}

void
BRN2RequestForwarder::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_param, (void *)H_DSR_STATS);
  add_read_handler("routemap", read_param, (void *)H_TRACK);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2RequestForwarder)
