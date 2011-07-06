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

#ifndef EVENTHANDLERELEMENT_HH
#define EVENTHANDLERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

CLICK_DECLS
/*
 * =c
 * EventHandler()
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

class EventHandler : public BRNElement {

 public:

  class DetectedEvent {
   public:
    EtherAddress _src;
    int _id;
    int _distance;

    DetectedEvent(EtherAddress src, int id, int distance = 0) {
      _src = src;
      _id = id;
      _distance = distance;
    }
  };

  typedef Vector<DetectedEvent> EventList;

 public:

  //
  //methods
  //
  EventHandler();
  ~EventHandler();

  const char *class_name() const  { return "EventHandler"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  String get_events();
  void reset();
 private:
  //
  //member
  //

  EventList _evli;

  void clear_eventlist();
  void add_event(EtherAddress src, int id, int distance);
  bool contains_event(EtherAddress src, int id);

 public:

  int _packet_events;
  int _dht_events;

};

CLICK_ENDDECLS
#endif
