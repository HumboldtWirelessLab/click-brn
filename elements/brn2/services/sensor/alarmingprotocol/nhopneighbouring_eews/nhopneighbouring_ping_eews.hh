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

#ifndef NHOPNEIGHBOURINGPINGEEWSELEMENT_HH
#define NHOPNEIGHBOURINGPINGEEWSELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"

#include "nhopneighbouring_info_eews.hh"

CLICK_DECLS
/*
 * =c
 * NHopNeighbouringPingEews()
 * =s
 * Output 0 : neighbouring packets
 * =d
 * this is a broadcast-based routing. IT does not implement the broadcast-forwarding itself
 * Unicast-packet from client or brn-node are encaped in broadcast-packets and send using
 * broadcast-flooding, which is an extra element
 */

#define DEFAULT_NHOPNEIGHBOURING_PING_EEWS_INTERVAL 5000

class NHopNeighbouringPingEews : public BRNElement {

 public:

  //
  //methods
  //
  NHopNeighbouringPingEews();
  ~NHopNeighbouringPingEews();

  const char *class_name() const  { return "NHopNeighbouringPingEews"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void run_timer(Timer *t);

  void trigger_event(int time);
  void handle_event();
 private:
  //
  //member
  //
  BRN2NodeIdentity *_node_identity;
  NHopNeighbouringInfoEews *nhop_info;
  AlarmingState *_as;


  void send_ping();

 public:
  Timer _timer;

 private:
  uint32_t _interval;
  bool _active;

};

CLICK_ENDDECLS
#endif
