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

#ifndef BRNBROADCASTROUTINGELEMENT_HH
#define BRNBROADCASTROUTINGELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"


CLICK_DECLS
/*
 * =c
 * BrnBroadcastRouting()
 * =s
 * Input 0  : Packets to route
 * Input 1  : BRNBroadcastRouting-Packets
 * Output 0 : Packets to local
 * Output 1 : BRNBroadcastRouting-Packets
 * =d
 * this is a broadcast-based routing. IT does not implement the broadcast-forwarding itself
 * Unicast-packet from client or brn-node are encaped in broadcast-packets and send using
 * broadcast-flooding, which is an extra element
 */

#define BROADCASTROUTING_DAFAULT_MAX_HOP_COUNT 100

class BrnBroadcastRouting : public BRNElement {

 public:

  //
  //methods
  //
  BrnBroadcastRouting();
  ~BrnBroadcastRouting();

  const char *class_name() const  { return "BrnBroadcastRouting"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //
 BRN2NodeIdentity *_node_id;

};

CLICK_ENDDECLS
#endif
