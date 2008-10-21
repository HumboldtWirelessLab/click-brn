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

#include "reqforwarder.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/wifi/ap/iapp/brniappstationtracker.hh"
#include "elements/brn/standard/brnpacketanno.hh"

CLICK_DECLS

RequestForwarder::RequestForwarder()
  : _debug(BrnLogger::DEFAULT),
  _me(),
  _link_table(),
  _dsr_decap(),
  _dsr_encap(),
  _brn_encap(),
  _client_assoc_lst(),
  _route_querier(),
  _sendbuffer_timer(this)
{
  //add_input(); //received rreq from another internal node
  //add_output(); //rreq has to be forwarded
  //add_output(); //reply to this respond (send to reply-forwarder)
}

RequestForwarder::~RequestForwarder()
{
}

int
RequestForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _iapp = NULL;

  if (cp_va_parse(conf, this, errh,
      cpOptional,
      cpElement, "NodeIdentity", &_me,
      cpElement, "DSRDecap", &_dsr_decap,
      cpElement, "DSREncap", &_dsr_encap,
      cpElement, "BRNEncap", &_brn_encap,
      cpElement, "RouteQuerier", &_route_querier,
      cpElement, "Client assoc list", &_client_assoc_lst,
      cpInteger, "Min metric for forwardd routerequest",&_min_metric_rreq_fwd,
      cpElement, "BRN iapp", &_iapp,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_dsr_decap || !_dsr_decap->cast("DSRDecap")) 
    return errh->error("DSRDecap not specified");

  if (!_dsr_encap || !_dsr_encap->cast("DSREncap")) 
    return errh->error("DSREncap not specified");

  if (!_brn_encap || !_brn_encap->cast("BRNEncap")) 
    return errh->error("BRNEncap not specified");

  if (!_route_querier || !_route_querier->cast("RouteQuerier")) 
    return errh->error("RouteQuerier not specified");

  if (!_client_assoc_lst || !_client_assoc_lst->cast("AssocList")) 
    return errh->error("ClientAssocList not specified");

  if ( _iapp ) {
    if ( !_iapp->cast("BrnIappStationTracker") )
      return errh->error("BrnIappStationTracker not specified");
  }

  return 0;
}

int
RequestForwarder::initialize(ErrorHandler *errh)
{
  _link_table = _me->get_link_table();
  if (!_link_table || !_link_table->cast("BrnLinkTable")) 
    return errh->error("BRNLinkTable not specified");

  unsigned int j = (unsigned int ) ( ( random() % ( JITTER ) ) );
//    BRN_DEBUG("RequestForwarder: Timer after %d ms", j );
  _sendbuffer_timer.initialize(this);
  _sendbuffer_timer.schedule_after_msec( j );

  return 0;
}

void
RequestForwarder::uninitialize()
{
  //cleanup
}

/* process an incoming route request packet */
void
RequestForwarder::push(int, Packet *p_in)
{
    String device(BRNPacketAnno::udevice_anno(p_in));

    BRN_DEBUG("* receiving dsr_rreq packet (packet_anno %s)", device.c_str());

    const click_brn_dsr *brn_dsr =
          (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

    assert(brn_dsr->dsr_type == BRN_DSR_RREQ);
    const click_brn_dsr_rreq dsr_rreq = brn_dsr->body.rreq;

    EtherAddress src_addr(brn_dsr->dsr_src.data); // src of the rreq
    EtherAddress dst_addr(brn_dsr->dsr_dst.data); // final dst of the rreq

    // fetch ip address of clients
    IPAddress src_ip_addr(brn_dsr->dsr_ip_src);
    IPAddress dst_ip_addr(brn_dsr->dsr_ip_dst);

    const click_ether *ether = (const click_ether *)p_in->ether_header();
    // rreq received from this node (last hop)
    EtherAddress prev_node(ether->ether_shost);

    BRN_DEBUG(" got route request for destination %s from %s with #ID %d",
        dst_addr.unparse().c_str(), prev_node.unparse().c_str(), ntohs(brn_dsr->body.rreq.dsr_id));

    int last_hop_metric;
    // get the so far estimated route from rreq
    RouteQuerierRoute request_route;
    // at this point the route is constructed from the src node to the last hop
    // SRC, HOP1, ..., LAST HOP
    _dsr_decap->extract_request_route(p_in, &last_hop_metric, request_route);

    BRN_DEBUG(" * Last_hop_metric %d, #ID %d", last_hop_metric, ntohs(brn_dsr->body.rreq.dsr_id));

    if ( last_hop_metric >= _min_metric_rreq_fwd ) { //BRN_DSR_MIN_METRIC_RREQ_FWD
      BRN_DEBUG(" Last_hop_metric %d is inferior as min_metric_rreq_fwd %d, #ID %d",
        last_hop_metric, _min_metric_rreq_fwd, ntohs(brn_dsr->body.rreq.dsr_id));
      BRN_DEBUG(" Kill Routerequest from junk link, #ID %d", ntohs(brn_dsr->body.rreq.dsr_id));
      p_in->kill();
      return;
    }

    // Check whether we have reached the final destination (the meshnode itself or an associated client station).
    // Continue the construction of the route up to the final destination
    // SRC, HOP1, ..., LAST HOP, THIS1, THIS2, DST
    if (_me->isIdentical(&dst_addr) || _client_assoc_lst->is_associated(dst_addr)) {

#ifdef CLICK_NS
      EtherAddress *device_addr = _me->getMyWirelessAddress(); // ethernet use wlan only in NS2
#else
      EtherAddress *device_addr = _me->getEthernetAddress(device); // ethernet addr of the interface the packet is coming from
#endif
      // estimate link metric to prev node
      int metric = _link_table->get_link_metric(prev_node, *device_addr);
      BRN_DEBUG("* RREQ: metric to prev node (%s) is %d.", prev_node.unparse().c_str(), metric);

      // the requested client is associated with this node
      if (_client_assoc_lst->is_associated(dst_addr)) {

        EtherAddress *my_wlan_addr = _me->getMyWirelessAddress();

        //clients are always connected via wireless (insert an inmemory hop)
        if ( (device == _me->getVlan0DeviceName()) || (device == _me->getVlan1DeviceName()) ) {
          BRN_DEBUG(" * addr of current node added to route(1): %s.", device_addr->unparse().c_str());

          //put myself into request route; use the metric to reach the client
          request_route.push_back(RouteQuerierHop(*device_addr, BRN_DSR_MEMORY_MEDIUM_METRIC));

          //put my wlan ether address into rreq
          request_route.push_back(RouteQuerierHop(*my_wlan_addr, 0)); // metric is not used
        } else {
          BRN_DEBUG(" * addr of current node added to route(2): %s.", my_wlan_addr->unparse().c_str());

          //put myself into request route; use the metric to reach the client
          request_route.push_back(RouteQuerierHop(*device_addr, 100)); // link to associated clients is always 100
        }
      } else if (_me->isIdentical(&dst_addr)) { //an internal node is the receiver
        BRN_DEBUG("* an internal node is the receiver; add myself to route.");

        // put my address into route
        request_route.push_back(RouteQuerierHop(*device_addr, 0)); // metric field is not used
      }
      BRN_DEBUG("* addr of assoc client/this node added to route.");
      // put the address of the client into route
      request_route.push_back(RouteQuerierHop(dst_addr, dst_ip_addr, 0)); // metric not used
    }

    BRN_DEBUG("* learned from RREQ ...");
    for (int j = 0; j < request_route.size(); j++)
      BRN_DEBUG(" RREQ - %d   %s (%d)",
                    j, request_route[j].ether().unparse().c_str(), request_route[j]._metric);

    // refresh own link table
    add_route_to_link_table(request_route);

    // sanity check - does a client reassociate?
    // TODO the information here is shit, leads to false roaming decisions

    EtherAddress first_ether = request_route[1].ether();

    if (true == _client_assoc_lst->is_associated(src_addr)
      && 1 <= request_route.size()
      && false == _me->isIdentical(&first_ether)) {

      if ( _iapp != NULL ) {
        // Remove the assocation from assoc_list
        _iapp->sta_roamed(src_addr, first_ether, *_me->getMyWirelessAddress());
      }
    }

    if (_me->isIdentical(&src_addr) || _client_assoc_lst->is_associated(src_addr)) {
      BRN_DEBUG("* I (=%s) sourced this RREQ; ignore., #ID %d",
          _me->getMyWirelessAddress()->unparse().c_str(), ntohs(brn_dsr->body.rreq.dsr_id));
      p_in->kill();
      return;
    } else if (_me->isIdentical(&dst_addr) || _client_assoc_lst->is_associated(dst_addr)) { // rreq reached destination  

      // this RREQ is for me, so generate a reply.
      RouteQuerierRoute reply_route;

      reverse_route(request_route, reply_route);

      BRN_DEBUG("* Generating route reply with source route");
      for (int j = 0; j < reply_route.size(); j++) {
        BRN_DEBUG(" - %d   %s (%d)",
                    j, reply_route[j].ether().unparse().c_str(),
                    reply_route[j]._metric);
      }

     int route_met = 0;
     for ( int j = 1; j < ( reply_route.size() - 1 ); j++)
     {
       route_met += reply_route[j]._metric;
     }
      // bitwise XOR
      int rreq_id = hashcode(src_addr) | dsr_rreq.dsr_id;

      if ( !_track_route.find(rreq_id))
     {
       _track_route.insert(rreq_id,route_met);
     }
     else
     {
       int last_route_met = _track_route.find(rreq_id);
       if ( last_route_met <= route_met ) 
       {
         BRN_DEBUG(" Seen Routerequest with better metric before; Drop packet! %d <= %d, #ID %d", last_route_met,
              route_met, ntohs(brn_dsr->body.rreq.dsr_id));
         p_in->kill(); // kill the original RREQ
         return;
       }
       else
       {
         _track_route.remove(rreq_id);
         _track_route.insert(rreq_id,route_met);
       }
     }

      // reply route is simply the inverted request route
      issue_rrep(dst_addr, dst_ip_addr, src_addr, src_ip_addr, reply_route, ntohs(brn_dsr->body.rreq.dsr_id));
      p_in->kill(); // kill the original RREQ

      return;

    } else { // this RREQ is not for me.  decide whether to forward it or just kill it.
      // reply from cache would also go here.
      // TODO: AZu: what about ttl???????????????
/*
      if (ip->ip_ttl == 1) {
        click_chatter(" * time to live expired; killing packet\n");
        p_in->kill();
        return;
      } // ttl is decremented in forward_rreq
*/	
      if (_me->findOwnIdentity(request_route) != -1) { // I am already listed
        // I'm in the route somewhere other than at the end
        BRN_DEBUG("* I'm already listed; killing packet, #ID %d",
            ntohs(brn_dsr->body.rreq.dsr_id));
        p_in->kill();
        return;
      }

      // check to see if we've seen this request lately, or if this
      // one is better
      ForwardedReqKey frk(src_addr, dst_addr, ntohs(dsr_rreq.dsr_id));
      ForwardedReqVal *old_frv = _route_querier->_forwarded_rreq_map.findp(frk);
// 
      // ETX:
      unsigned short this_metric = _route_querier->route_metric(request_route);

      // ETX not yet used
      if (old_frv) {
        if (_debug == BrnLogger::DEBUG) {
          BRN_DEBUG("* already forwarded this route request %d (%d, %d)",
             ntohs(dsr_rreq.dsr_id), this_metric, old_frv->best_metric);
          if (_route_querier->metric_preferable(this_metric, old_frv->best_metric)) {
            BRN_DEBUG(" * but this one is better");
          } else {
            BRN_DEBUG("* and this one's not as good");
          }
        }
      }

      if (old_frv && ! _route_querier->metric_preferable(this_metric, old_frv->best_metric)) { 
        BRN_DEBUG("* already forwarded this route request. #ID %d", ntohs(brn_dsr->body.rreq.dsr_id));
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

        //EtherAddress last_forwarder = EtherAddress(DSR_LAST_HOP_IP_ANNO(p_in));
        // or:
        EtherAddress last_forwarder = request_route[request_route.size()-2].ether();

        BRN_DEBUG("* last_forwarder is %s", last_forwarder.unparse().c_str());

        if (old_frv && old_frv->p) {
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
          old_frv->p->kill();
          old_frv->p = NULL;
        }
        BRN_DEBUG("* forwarding this RREQ %s %s", device.c_str(), (BRNPacketAnno::udevice_anno(p_in)).c_str());

        new_frv.p = NULL;
        new_frv.best_metric = this_metric;
        _route_querier->_forwarded_rreq_map.insert(frk, new_frv);
        forward_rreq(p_in);

        return; 
      }
    }
}

/*
 * rebroadcast a route request.  we've already checked that we haven't
 * seen this RREQ lately, and added the info to the forwarded_rreq_map.
 *
 */
void
RequestForwarder::forward_rreq(Packet *p_in)
{
  String device(BRNPacketAnno::udevice_anno(p_in));
  // add my address to the end of the packet
  WritablePacket *p=p_in->uniqueify();

  click_brn *brn = (click_brn *)p->data();
  click_brn_dsr *dsr_rreq = (click_brn_dsr *)(p->data() + sizeof(click_brn));

  int hop_count = dsr_rreq->dsr_hop_count;

  BRN_DEBUG("* RequestForwarder::forward_rreq with #ID %d", ntohs(dsr_rreq->body.rreq.dsr_id));

  if (hop_count >= BRN_DSR_MAX_HOP_COUNT) {
    BRN_DEBUG("* Maximum hop count reached in rreq; drop packet.");
    return;
  }

  // new hop is the address of the last node
  //EtherAddress hop(ether->ether_shost);
  click_ether *ether = (click_ether *)p->ether_header();

  // rreq received from this node ...
  EtherAddress prev_node(ether->ether_shost);

  memcpy(dsr_rreq->addr[hop_count].hw.data, (uint8_t *)prev_node.data(), 6 * sizeof(uint8_t));

  //rreq is a broadcast; use the ether address associated with packet's device
  EtherAddress *me = _me->getEthernetAddress(device);

  if (me) {
    // set the metric no my previous node
    int metric = _link_table->get_link_metric(prev_node, *me);
    BRN_DEBUG("* append prev node (%s) to rreq with metric %d.", prev_node.unparse().c_str(), metric);
    dsr_rreq->addr[hop_count].metric = htons(metric);
  } else {
    BRN_DEBUG("* device unknown: %s", device.c_str());
  }

  // update hop count
  dsr_rreq->dsr_hop_count++;

  // reduce ttl
  brn->ttl--;

  // set destination anno
  BRNPacketAnno::set_dst_ether_anno(p,EtherAddress((const unsigned char *)"\xff\xff\xff\xff\xff\xff"));

  // copy device anno
  BRNPacketAnno::set_udevice_anno(p,device.c_str());
  BRN_DEBUG(" * current dev_anno %s.", device.c_str());

  // Buffering + Jitter
  unsigned int j = (unsigned int ) ( ( random() % ( JITTER ) ) );
  _packet_queue.push_back( BufferedPacket( p , j ));

  j = get_min_jitter_in_queue();

  _sendbuffer_timer.schedule_after_msec(j);
}


/* method generates and sends a dsr route reply */
void
RequestForwarder::issue_rrep(EtherAddress src, IPAddress src_ip, EtherAddress dst,
			  IPAddress dst_ip, const RouteQuerierRoute &reply_route, uint16_t rreq_id)
{
  BRN_DEBUG("* issue_rrep: ... #ID %d", rreq_id);
  Packet *brn_p;

  //prepend dsr header
  Packet *rrep_p = _dsr_encap->create_rrep(dst, dst_ip, src, src_ip, reply_route, rreq_id);
  //prepend brn header
  assert(rrep_p);
  brn_p = _brn_encap->add_brn_header(rrep_p);

  //output route reply packet
  output(1).push(brn_p);
}

//-----------------------------------------------------------------------------
// Timer methods
//-----------------------------------------------------------------------------

void
RequestForwarder::run_timer(Timer *)
{
  queue_timer_hook();
}

void
RequestForwarder::queue_timer_hook()
{
  struct timeval curr_time;
  curr_time = Timestamp::now().timeval();

  for ( int i = 0; i < _packet_queue.size(); i++)
  {
    if ( diff_in_ms( _packet_queue[i]._send_time, curr_time ) <= 0 )
    {
      Packet *out_packet = _packet_queue[i]._p;
      _packet_queue.erase(_packet_queue.begin() + i);
      output(0).push( out_packet );
      break;
    }
  }

  unsigned int j = get_min_jitter_in_queue();
  if ( j < MIN_JITTER ) j = MIN_JITTER;

//  if (_debug)
//     click_chatter("RequestForwarder: Timer after %d ms", j );
 
  _sendbuffer_timer.schedule_after_msec(j);
}

long
RequestForwarder::diff_in_ms(timeval t1, timeval t2)
{
  long s, us, ms;

  while (t1.tv_usec < t2.tv_usec) {
    t1.tv_usec += 1000000;
    t1.tv_sec -= 1;
  }

  s = t1.tv_sec - t2.tv_sec;

  us = t1.tv_usec - t2.tv_usec;
  ms = s * 1000L + us / 1000L;

  return ms;
}

unsigned int
RequestForwarder::get_min_jitter_in_queue()
{
  struct timeval _next_send;
  struct timeval _time_now;
  long next_jitter;

  if ( _packet_queue.size() == 0 ) return( 1000 );
  else
  {
    _next_send.tv_sec = _packet_queue[0]._send_time.tv_sec;
    _next_send.tv_usec = _packet_queue[0]._send_time.tv_usec;

    for ( int i = 1; i < _packet_queue.size(); i++ )
    {
      if ( ( _next_send.tv_sec > _packet_queue[i]._send_time.tv_sec ) ||
           ( ( _next_send.tv_sec == _packet_queue[i]._send_time.tv_sec ) && (  _next_send.tv_usec > _packet_queue[i]._send_time.tv_usec ) ) )
      {
        _next_send.tv_sec = _packet_queue[i]._send_time.tv_sec;
        _next_send.tv_usec = _packet_queue[i]._send_time.tv_usec;
      }
    }

    _time_now = Timestamp::now().timeval();

    next_jitter = diff_in_ms(_next_send, _time_now);

    if ( next_jitter <= 1 ) return( 1 );
    else return( next_jitter );   
 }
}



//-----------------------------------------------------------------------------
// Helper methods
//-----------------------------------------------------------------------------

void
RequestForwarder::add_route_to_link_table(const RouteQuerierRoute &route)
{

  for (int i=0; i < route.size() - 1; i++) {
    EtherAddress ether1 = route[i].ether();
    EtherAddress ether2 = route[i+1].ether();

    if (ether1 == ether2)
      continue;

    uint16_t metric = route[i]._metric; //metric starts with no offset

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
      BRN_DEBUG(" _link_table->update_link %s (%s) %s (%s) %d",
        route[i].ether().unparse().c_str(), route[i].ip().unparse().c_str(),
        route[i+1].ether().unparse().c_str(), route[i+1].ip().unparse().c_str(), metric);
  }
}

void
RequestForwarder::reverse_route(const RouteQuerierRoute &in, RouteQuerierRoute &out)
{
  for(int i=in.size()-1; i>=0; i--) {
    out.push_back(in[i]);
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  RequestForwarder *rf = (RequestForwarder *)e;
  return String(rf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  RequestForwarder *rf = (RequestForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  rf->_debug = debug;
  return 0;
}

void
RequestForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

#include <click/bighashmap.cc>
#include <click/vector.cc>
template class Vector<RequestForwarder::BufferedPacket>;

CLICK_ENDDECLS
EXPORT_ELEMENT(RequestForwarder)
