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
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"

CLICK_DECLS

BRN2DSRDecap::BRN2DSRDecap()
  : _debug(BrnLogger::DEFAULT),
  _link_table(),
  _me()
{
}

BRN2DSRDecap::~BRN2DSRDecap()
{
}

int
BRN2DSRDecap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "LINKTABLE", cpkP+cpkM, cpElement,  &_link_table,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity"))
    return errh->error("NodeIdentity not specified");

  if (!_link_table || !_link_table->cast("Brn2LinkTable"))
    return errh->error("BRNLinkTable not specified");

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
void
BRN2DSRDecap::extract_request_route(const Packet *p_in, int *ref_metric, BRN2RouteQuerierRoute &route)
{
  BRN_DEBUG(" * extract_request_route from dsr packet.");

  String device(BRNPacketAnno::udevice_anno((Packet *)p_in));
  // address of the node originating the rreq
  const click_brn_dsr *dsr_rreq =
      (const click_brn_dsr *)(p_in->data() + sizeof(click_brn));
  EtherAddress src_addr(dsr_rreq->dsr_src.data);
  EtherAddress dst_addr(dsr_rreq->dsr_dst.data);

  // fetch ip address of clients
  IPAddress src_ip_addr(dsr_rreq->dsr_ip_src);
  IPAddress dst_ip_addr(dsr_rreq->dsr_ip_dst);

  BRN_DEBUG(" ** IP-Clients %s(%s) --> %s(%s)",
        src_addr.unparse().c_str(), src_ip_addr.unparse().c_str(), dst_addr.unparse().c_str(), dst_ip_addr.unparse().c_str());

  assert(dsr_rreq->dsr_type == BRN_DSR_RREQ);

  int num_addr = dsr_rreq->dsr_hop_count;

  BRN_DEBUG(" * hop count in RREQ so far is %d.", num_addr);

  // route is { dsr_src, addr[0], addr[1], ..., dsr_dst }
  // put the originator of this rreq into dsr route

  /*
   * TODO: we have following problem: We need to obtain the metric between the first two ...
   */
  route.push_back(RouteQuerierHop(src_addr, src_ip_addr, 100));

  for (int i=0; i<num_addr; i++) {
    click_dsr_hop hop = dsr_rreq->addr[i];
    BRN_DEBUG(" * extract route %s with m=%d", EtherAddress(hop.hw.data).unparse().c_str(), ntohs(hop.metric));
    route.push_back(RouteQuerierHop(hop.hw, ntohs(hop.metric)));
  }

  // put the previous node into route list
  const click_ether *ether = (click_ether *)p_in->ether_header();
  assert(ether);
  // ethernet address of the last hop
  EtherAddress last_node_addr(ether->ether_shost);

  uint8_t devicenumber = BRNPacketAnno::devicenumber_anno(p_in);
  BRN2Device *indev = _me->getDeviceByNumber(devicenumber);
  EtherAddress *my_rec_addr = indev->getEtherAddress(); // ethernet addr of the interface the packet is coming from

  int metric = _link_table->get_link_metric(last_node_addr, *my_rec_addr);
  *ref_metric = metric;

  BRN_DEBUG(_link_table->print_links().c_str());
  BRN_DEBUG(" * my (%s) metric for last hop (%s) is) %d", my_rec_addr->unparse().c_str(), last_node_addr.unparse().c_str(), metric);

  route.push_back(RouteQuerierHop(last_node_addr, metric));
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
  const click_brn_dsr *dsr
      = (const click_brn_dsr *)(p->data() + sizeof(click_brn));
  EtherAddress dest_ether(dsr->dsr_dst.data); //destination of route reply
  EtherAddress src_ether(dsr->dsr_src.data); // source of route reply

  // fetch ip address of clients
  IPAddress src_ip_addr(dsr->dsr_ip_src);
  IPAddress dst_ip_addr(dsr->dsr_ip_dst);

  BRN_DEBUG(" ** IP-Clients %s(%s) --> %s(%s)",
        src_ether.unparse().c_str(), src_ip_addr.unparse().c_str(), dest_ether.unparse().c_str(), dst_ip_addr.unparse().c_str());

  assert(dsr->dsr_type == BRN_DSR_RREP);

  int hop_count = dsr->dsr_hop_count;

  BRN_DEBUG(" * extracting route from %d-hop route reply.", hop_count);

  // construct the route from the reply addresses.
  route.push_back(RouteQuerierHop(src_ether, src_ip_addr, 0)); //metric value makes no sense here

  // we have to update all links between us and our neighbors used in route reply
  for(int i = 0; i < hop_count; i++) { // collect all hops
    route.push_back(RouteQuerierHop(dsr->addr[i].hw, ntohs(dsr->addr[i].metric)));
  }

  route.push_back(RouteQuerierHop(dest_ether, dst_ip_addr, 0)); // metric is not used
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
  const click_brn_dsr *dsr
      = (const click_brn_dsr *)(p_in->data() + sizeof(click_brn));

  int num_addr = dsr->dsr_hop_count;

  BRN_DEBUG(" * hop count so far is %d.", num_addr);

  //assert((dsr->dsr_type == BRN_DSR_SRC) || (dsr->dsr_type == BRN_DSR_RREP));

  EtherAddress src_addr(dsr->dsr_src.data);
  EtherAddress dst_addr(dsr->dsr_dst.data);

  // fetch ip address of clients
  IPAddress src_ip_addr(dsr->dsr_ip_src);
  IPAddress dst_ip_addr(dsr->dsr_ip_dst);

  BRN_DEBUG(" ** IP-Clients %s(%s) --> %s(%s).",
        src_addr.unparse().c_str(), src_ip_addr.unparse().c_str(), dst_addr.unparse().c_str(), dst_ip_addr.unparse().c_str());

  // put the originator of this rreq into dsr route
  route.push_back(RouteQuerierHop(src_addr, src_ip_addr, 0)); //TODO Metric not used

  for (int i = 0; i < num_addr; i++) {
    click_dsr_hop hop = dsr->addr[i];
    route.push_back(RouteQuerierHop(hop.hw, ntohs(hop.metric)));
    EtherAddress eth_ = EtherAddress(hop.hw.data);
    BRN_DEBUG("Adress: %s Metric: %d", eth_.unparse().c_str(), ntohs(hop.metric));
  }

  // put the originator of this rreq into dsr route
  route.push_back(RouteQuerierHop(dst_addr, dst_ip_addr, 0)); //TODO Metric not used

  BRN_DEBUG(" * extract_source_route : size %d", route.size());
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRN2DSRDecap *dd = (BRN2DSRDecap *)e;
  return String(dd->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRN2DSRDecap *dd = (BRN2DSRDecap *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  dd->_debug = debug;
  return 0;
}

void
BRN2DSRDecap::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DSRDecap)
