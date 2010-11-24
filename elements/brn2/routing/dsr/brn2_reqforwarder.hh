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

#ifndef BRN2REQUESTFORWARDERELEMENT_HH
#define BRN2REQUESTFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

#include "elements/brn2/wifi/ap/brn2_assoclist.hh"
#include "elements/brn2/standard/packetsendbuffer.hh"

#include "brn2_routequerier.hh"
#include "brn2_dsrdecap.hh"
#include "brn2_dsrencap.hh"

CLICK_DECLS

#define JITTER 17
#define MIN_JITTER 5

class BRN2DSREncap;
class BRN2RouteQuerier;


/*
 * =c
 * BRN2RequestForwarder()
 * =s forwards rreq packets
 * forwards dsr route request packets
 * output 0: route reply
 * output 1: route request
 * =d
 */
class BRN2RequestForwarder : public BRNElement {

 public:
  //
  //methods
  //
  BRN2RequestForwarder();
  ~BRN2RequestForwarder();

  const char *class_name() const  { return "BRN2RequestForwarder"; }
  const char *port_count() const  { return "1/2"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  void push(int, Packet *);

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

  void forward_rreq(Packet *p_in, EtherAddress *detour_nb, int detour_metric_last_nb);

  void run_timer(Timer *timer);

public: 
  //
  //member
  //
  BRN2NodeIdentity *_me;
  Brn2LinkTable *_link_table;
  BRN2DSRDecap *_dsr_decap;
  BRN2DSREncap *_dsr_encap;
  BRN2RouteQuerier *_route_querier;

private:
  int _min_metric_rreq_fwd;

// keep track of already forwarded RREQ; forward only route request with better metric
// TODO mal ordentlich machen (Timeout)
  HashMap<uint32_t, int> _track_route;

  //
  //Timer methods
  PacketSendBuffer _packet_queue;
  Timer _sendbuffer_timer;
  void queue_timer_hook();

  //
  //methods
  //
  void reverse_route(const BRN2RouteQuerierRoute &in, BRN2RouteQuerierRoute &out);
  void issue_rrep(EtherAddress, IPAddress, EtherAddress, IPAddress, const BRN2RouteQuerierRoute &, uint16_t rreq_id);
  int findOwnIdentity(const BRN2RouteQuerierRoute &r);

  bool _enable_last_hop_optimization;
  bool _enable_full_route_optimization;

  bool _enable_delay_queue;

 public:
  int _stats_receive_better_route;
  int _stats_avoid_bad_route_forward;

};

CLICK_ENDDECLS
#endif
