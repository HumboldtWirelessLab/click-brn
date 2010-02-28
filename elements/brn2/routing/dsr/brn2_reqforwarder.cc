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
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "brn2_dsrprotocol.hh"
#include "brn2_reqforwarder.hh"

CLICK_DECLS

BRN2RequestForwarder::BRN2RequestForwarder()
  : _debug(BrnLogger::DEFAULT),
  _me(),
  _link_table(),
  _dsr_decap(),
  _dsr_encap(),
//  _brn_encap(),
  _route_querier(),
  _sendbuffer_timer(this)
{
  //add_input(); //received rreq from another internal node
  //add_output(); //rreq has to be forwarded
  //add_output(); //reply to this respond (send to reply-forwarder)
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
//      "BRNENCAP", cpkP+cpkM, cpElement, &_brn_encap,
      "ROUTEQUERIER", cpkP+cpkM, cpElement, &_route_querier,
      "MINMETRIC", cpkP+cpkM, cpInteger, &_min_metric_rreq_fwd,
      "DEBUG", cpkP+cpkM, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_dsr_decap || !_dsr_decap->cast("BRN2DSRDecap")) 
    return errh->error("DSRDecap not specified");

  if (!_dsr_encap || !_dsr_encap->cast("BRN2DSREncap"))
    return errh->error("DSREncap not specified");

//  if (!_brn_encap || !_brn_encap->cast("BRN2Encap")) 
//    return errh->error("BRNEncap not specified");

  if (!_route_querier || !_route_querier->cast("BRN2RouteQuerier")) 
    return errh->error("RouteQuerier not specified");

  return 0;
}

int
BRN2RequestForwarder::initialize(ErrorHandler *)
{

  unsigned int j = (unsigned int ) ( ( click_random() % ( JITTER ) ) );
//    BRN_DEBUG("BRN2RequestForwarder: Timer after %d ms", j );
  _sendbuffer_timer.initialize(this);
  _sendbuffer_timer.schedule_after_msec( j );

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


/* process an incoming route request packet */
void
BRN2RequestForwarder::push(int, Packet *p_in)
{
    BRN2Device *indev;
    uint8_t devicenumber = BRNPacketAnno::devicenumber_anno(p_in);
    EtherAddress *device_addr;

    BRN_DEBUG("* receiving dsr_rreq packet (packet_anno %s)", _me->getDeviceByNumber(devicenumber)->getDeviceName().c_str());

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
    BRN2RouteQuerierRoute request_route;
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
    if ( _me->isIdentical(&dst_addr) || ( _link_table->is_associated(dst_addr)) ) {

      BRN2Device *indev = _me->getDeviceByNumber(devicenumber);
      const EtherAddress *device_addr = indev->getEtherAddress(); // ethernet addr of the interface the packet is coming from

      // estimate link metric to prev node
      int metric = _link_table->get_link_metric(prev_node, *device_addr);
      BRN_DEBUG("* RREQ: metric to prev node (%s) is %d.", prev_node.unparse().c_str(), metric);

      // the requested client is associated with this node
      if (_link_table->is_associated(dst_addr)) {

        click_chatter("er soll zu mir gehoeren");
            BRN_DEBUG(" * Linktable at shutdown: %s", _link_table->print_links().c_str());

        EtherAddress *my_wlan_addr = _link_table->get_neighbor(dst_addr); //this should give back only one etheraddress if the client is associated

        BRN_DEBUG(" * addr of current node added to route(2): %s.", my_wlan_addr->unparse().c_str());

          //put myself into request route; use the metric to reach the client
        request_route.push_back(BRN2RouteQuerierHop(*device_addr, 100)); // link to associated clients is always 100

      } else if (_me->isIdentical(&dst_addr)) { //an internal node is the receiver
        BRN_DEBUG("* an internal node is the receiver; add myself to route.");

        // put my address into route
        request_route.push_back(BRN2RouteQuerierHop(*device_addr, 0)); // metric field is not used
      }
      BRN_DEBUG("* addr of assoc client/this node added to route.");
      // put the address of the client into route
      request_route.push_back(BRN2RouteQuerierHop(dst_addr, dst_ip_addr, 0)); // metric not used
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

/*    if (( _link_table->get_host_metric_to_me(src_addr) == 50  )
      && 1 <= request_route.size()
      && false == _me->isIdentical(&first_ether)) {

    }
*/
    if (_me->isIdentical(&src_addr) || (_link_table->is_associated(src_addr))) {
      BRN2Device *indev = _me->getDeviceByNumber(devicenumber);
      const EtherAddress *device_addr = indev->getEtherAddress(); // ethernet addr of the interface the packet is coming from

      BRN_DEBUG("* I (=%s) sourced this RREQ; ignore., #ID %d",
                   device_addr->unparse().c_str(), ntohs(brn_dsr->body.rreq.dsr_id));
      p_in->kill();
      return;
    } else if (_me->isIdentical(&dst_addr) || ( _link_table->is_associated(dst_addr))) { // rreq reached destination

      // this RREQ is for me, so generate a reply.
      BRN2RouteQuerierRoute reply_route;

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
      if (findOwnIdentity(request_route) != -1) { // I am already listed
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

        indev = _me->getDeviceByNumber(devicenumber);
        const EtherAddress *device_addr = indev->getEtherAddress(); // ethernet addr of the interface the packet is coming from

        BRN_DEBUG("* forwarding this RREQ %s %s", indev->getDeviceName().c_str(), (BRNPacketAnno::udevice_anno(p_in)).c_str());

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
BRN2RequestForwarder::forward_rreq(Packet *p_in)
{
  uint8_t devicenumber = BRNPacketAnno::devicenumber_anno(p_in);
  BRN2Device *indev;

  // add my address to the end of the packet
  WritablePacket *p_u=p_in->uniqueify();

  WritablePacket *p = DSRProtocol::extend_hops(p_u,1);  //add space for one additional hop

  BRN_DEBUG("Headersize: %d brn+dsr: %d",p->length(),sizeof(click_brn) + sizeof(click_brn_dsr));

  click_brn *brn = (click_brn *)p->data();
  click_brn_dsr *dsr_rreq = (click_brn_dsr *)(p->data() + sizeof(click_brn));

  int hop_count = dsr_rreq->dsr_hop_count;

  BRN_DEBUG("* BRN2RequestForwarder::forward_rreq with #ID %d", ntohs(dsr_rreq->body.rreq.dsr_id));

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

  //rreq is a broadcast; use the ether address associated with packet's device
  indev = _me->getDeviceByNumber(devicenumber);
  const EtherAddress *me = indev->getEtherAddress(); // ethernet addr of the interface the packet is coming from

  if (me) {
    // set the metric no my previous node
    int metric = _link_table->get_link_metric(prev_node, *me);
    BRN_DEBUG("* append prev node (%s) to rreq with metric %d.", prev_node.unparse().c_str(), metric);
    dsr_hops[hop_count].metric = htons(metric);
  } else {
    BRN_DEBUG("* device unknown: %s", indev->getDeviceName().c_str());
  }

  // update hop count
  dsr_rreq->dsr_hop_count++;

  // reduce ttl
  brn->ttl--;

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

  // Buffering + Jitter
  unsigned int j = (unsigned int ) ( ( click_random() % ( JITTER ) ) );
  _packet_queue.push_back( BufferedPacket( p , j ));

  j = get_min_jitter_in_queue();

  _sendbuffer_timer.schedule_after_msec(j);
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

//-----------------------------------------------------------------------------
// Timer methods
//-----------------------------------------------------------------------------

void
BRN2RequestForwarder::run_timer(Timer *)
{
  queue_timer_hook();
}

void
BRN2RequestForwarder::queue_timer_hook()
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
//     click_chatter("BRN2RequestForwarder: Timer after %d ms", j );
 
  _sendbuffer_timer.schedule_after_msec(j);
}

long
BRN2RequestForwarder::diff_in_ms(timeval t1, timeval t2)
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
BRN2RequestForwarder::get_min_jitter_in_queue()
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
BRN2RequestForwarder::add_route_to_link_table(const BRN2RouteQuerierRoute &route)
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
BRN2RequestForwarder::reverse_route(const BRN2RouteQuerierRoute &in, BRN2RouteQuerierRoute &out)
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
  BRN2RequestForwarder *rf = (BRN2RequestForwarder *)e;
  return String(rf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRN2RequestForwarder *rf = (BRN2RequestForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  rf->_debug = debug;
  return 0;
}

void
BRN2RequestForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

#include <click/bighashmap.cc>
#include <click/vector.cc>
template class Vector<BRN2RequestForwarder::BufferedPacket>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2RequestForwarder)
