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
 * dsrdecap.{cc,hh} -- strips header from dsr packet
 * A. Zubow
 */

#include <click/config.h>

#include "brn2_dsrdecap.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/brn2.h"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

BRN2DSRDecap::BRN2DSRDecap()
{
  BRNElement::init();
}

BRN2DSRDecap::~BRN2DSRDecap()
{
}

int
BRN2DSRDecap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
BRN2DSRDecap::initialize(ErrorHandler *)
{
  return 0;
}

/*
 * Takes a RREQ packet, and returns the route which that request has
 * so far accumulated.  (we then take this route and add it to the
 * link cache)
 */
uint16_t
BRN2DSRDecap::extract_request_route(const Packet *p_in, BRN2RouteQuerierRoute &route,
                                    EtherAddress *last_hop, uint16_t last_hop_metric )
{
  uint16_t route_metric = 0;

  BRN_DEBUG(" * extract_request_route from dsr packet.");

  // address of the node originating the rreq
  click_brn_dsr *dsr_rreq = (click_brn_dsr *)(p_in->data() + sizeof(click_brn));
  EtherAddress src_addr(dsr_rreq->dsr_src.data);
  EtherAddress dst_addr(dsr_rreq->dsr_dst.data);

  // fetch ip address of clients
  IPAddress src_ip_addr(dsr_rreq->dsr_ip_src);
  IPAddress dst_ip_addr(dsr_rreq->dsr_ip_dst);

  BRN_DEBUG(" ** IP-Clients %s(%s) --> %s(%s)", src_addr.unparse().c_str(), src_ip_addr.unparse().c_str(),
                                                dst_addr.unparse().c_str(), dst_ip_addr.unparse().c_str());

  assert(dsr_rreq->dsr_type == BRN_DSR_RREQ);

  int num_addr = dsr_rreq->dsr_hop_count;

  BRN_DEBUG(" * hop count in RREQ so far is %d.", num_addr);

  // route is { dsr_src, addr[0], addr[1], ..., dsr_dst }
  // put the originator of this rreq into dsr route

  /*
   * TODO: we have following problem: We need to obtain the metric between the first two ...
   */
  route.push_back(BRN2RouteQuerierHop(src_addr, src_ip_addr, BRN_LT_MEMORY_MEDIUM_METRIC));

  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(dsr_rreq);
  for (int i = 0; i < num_addr; i++) {
    click_dsr_hop hop = dsr_hops[i]; //dsr_rreq->addr[i];  //RobAt:DSR
    BRN_DEBUG(" * extract route %s with m=%d", EtherAddress(hop.hw.data).unparse().c_str(), ntohs(hop.metric));
    route.push_back(BRN2RouteQuerierHop(hop.hw, ntohs(hop.metric)));
    route_metric += ntohs(hop.metric);
  }

  route.push_back(BRN2RouteQuerierHop(*last_hop, last_hop_metric));

  route_metric += last_hop_metric;

  return route_metric;
}

/*
 * Takes an RREP packet, copies out the reply route and returns it
 * as a Vector<RouteQuerierHop>.
 */
void
BRN2DSRDecap::extract_reply_route(const Packet *p, BRN2RouteQuerierRoute &route)
{

  BRN_DEBUG(" * extract_reply_route from dsr packet.");

  // constructed route
  //RouteQuerierRoute route;

  // get the right header
  /*const*/ click_brn_dsr *dsr //RobAt:DSR
      = (click_brn_dsr *)(p->data() + sizeof(click_brn));
  EtherAddress dest_ether(dsr->dsr_dst.data); //destination of route reply
  EtherAddress src_ether(dsr->dsr_src.data); // source of route reply

  // fetch ip address of clients
  IPAddress src_ip_addr(dsr->dsr_ip_src);
  IPAddress dst_ip_addr(dsr->dsr_ip_dst);

  BRN_DEBUG(" ** IP-Clients %s(%s) --> %s(%s)", src_ether.unparse().c_str(), src_ip_addr.unparse().c_str(),
                                                dest_ether.unparse().c_str(), dst_ip_addr.unparse().c_str());

  assert(dsr->dsr_type == BRN_DSR_RREP);

  int hop_count = dsr->dsr_hop_count;

  BRN_DEBUG(" * extracting route from %d-hop route reply.", hop_count);

  // construct the route from the reply addresses.
  route.push_back(BRN2RouteQuerierHop(src_ether, src_ip_addr, 0)); //metric value makes no sense here

  // we have to update all links between us and our neighbors used in route reply
  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(dsr);

  for(int i = 0; i < hop_count; i++) { // collect all hops
    route.push_back(BRN2RouteQuerierHop(dsr_hops[i].hw, ntohs(dsr_hops[i].metric)));
  }

  route.push_back(BRN2RouteQuerierHop(dest_ether, dst_ip_addr, 0)); // metric is not used
}

/*
 * Returns the source route from this packet; used for producing route
 * error messages (extract route, truncate at my address, reverse, and
 * use that as the source route for the route error)
 */
void
BRN2DSRDecap::extract_source_route(const Packet *p_in, BRN2RouteQuerierRoute &route)
{
  BRN_DEBUG(" * extract_source_route from dsr packet.");
  // route is { dsr_src, addr[0], addr[1], ..., dsr_dst }
  //RouteQuerierRoute route;
  /*const*/ click_brn_dsr *dsr //RobAt:DSR
      = (/*const*/ click_brn_dsr *)(p_in->data() + sizeof(click_brn));

  int num_addr = dsr->dsr_hop_count;

  BRN_DEBUG(" * hop count so far is %d.", num_addr);

  //assert((dsr->dsr_type == BRN_DSR_SRC) || (dsr->dsr_type == BRN_DSR_RREP));

  EtherAddress src_addr(dsr->dsr_src.data);
  EtherAddress dst_addr(dsr->dsr_dst.data);

  // fetch ip address of clients
  IPAddress src_ip_addr(dsr->dsr_ip_src);
  IPAddress dst_ip_addr(dsr->dsr_ip_dst);

  BRN_DEBUG(" ** IP-Clients %s(%s) --> %s(%s).",
        src_addr.unparse().c_str(), src_ip_addr.unparse().c_str(),
        dst_addr.unparse().c_str(), dst_ip_addr.unparse().c_str());

  // put the originator of this rreq into dsr route
  route.push_back(BRN2RouteQuerierHop(src_addr, src_ip_addr, 0)); //TODO Metric not used

  click_dsr_hop *dsr_hops = DSRProtocol::get_hops(dsr);//RobAt:DSR

  for (int i = 0; i < num_addr; i++) {
    click_dsr_hop hop = dsr_hops[i]; //dsr->addr[i];
    route.push_back(BRN2RouteQuerierHop(hop.hw, ntohs(hop.metric)));
    EtherAddress eth_ = EtherAddress(hop.hw.data);
    BRN_DEBUG("Adress: %s Metric: %d", eth_.unparse().c_str(), ntohs(hop.metric));
  }

  // put the originator of this rreq into dsr route
  route.push_back(BRN2RouteQuerierHop(dst_addr, dst_ip_addr, 0)); //TODO Metric not used

  BRN_DEBUG(" * extract_source_route : size %d", route.size());
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BRN2DSRDecap::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DSRDecap)
