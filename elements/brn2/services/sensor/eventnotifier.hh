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

#ifndef EVENTNOTIFIERELEMENT_HH
#define EVENTNOTIFIERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/dht/storage/dhtstorage.hh"

CLICK_DECLS
/*
 * =c
 * EventNotifier()
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

class EventNotifier : public BRNElement {

 public:

  //
  //methods
  //
  EventNotifier();
  ~EventNotifier();

  const char *class_name() const  { return "EventNotifier"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/1-2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );
  Packet *pull( int port );

  int initialize(ErrorHandler *);
  void add_handlers();

  void run_timer(Timer *t);

  void trigger_event(int time);
  void handle_event();

 private:
  //
  //member
  //
  int _payload_size;

 public:

  void set_payload_size(int payload_size) { _payload_size = payload_size; }

  Timer _timer;

  EtherAddress _me;
  EtherAddress _eventhandleraddr;
  int _handler_events;
  int _push_packet_events;
  int _pull_packet_events;

  DHTStorage *_dht_storage;

  int _id;

};

CLICK_ENDDECLS
#endif
