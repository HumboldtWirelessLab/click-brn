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

#ifndef HAWKFORWARDERELEMENT_HH
#define HAWKFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>

#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/dht/protocol/dhtprotocol.hh"
#include "elements/brn/dht/routing/falcon/dhtrouting_falcon.hh"
#include "elements/brn/dht/routing/falcon/dhtprotocol_falcon.hh"
#include "elements/brn/dht/routing/falcon/falcon_functions.hh"
#include "hawk_routingtable.hh"
#include "elements/brn/dht/routing/falcon/falcon_routingtable.hh"


CLICK_DECLS

/*
 * =c
 * HawkForwarder()
 * =s forwards src packets
 * forwards dsr source routed packets
 * =d
 */
class HawkForwarder : public Element {

 public:
  //
  //member
  //
  int _debug;
  //OPTIMIZATION-FLAGS

  bool _opt_first_dst;
  bool _opt_better_finger;
  bool _opt_successor_forward;
  //
  //methods
  //
  HawkForwarder();
  ~HawkForwarder();

  const char *class_name() const  { return "HawkForwarder"; }
  const char *port_count() const  { return "2/2"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  void push(int, Packet *);

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

 private:
  //
  //member
  //
  BRN2NodeIdentity *_me;
  DHTRoutingFalcon *_falconrouting;
  HawkRoutingtable *_rt;
  FalconRoutingTable *_frt;

};

CLICK_ENDDECLS
#endif
