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

#ifndef SETSOURCEANDOUTPUTFORDEVICEELEMENT_HH
#define SETSOURCEANDOUTPUTFORDEVICEELEMENT_HH

#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/element.hh>
#include "brn.h"
#include "nodeidentity.hh"
#include "assoclist.hh"
#include "brnlinktable.hh"

CLICK_DECLS
/*
 * =c
 * SetSourceAndOutputForDevice()
 * =s Find out the appropiate device from the packets destination address, set source address according to device
 * and output for device. Broadcasts are outputted on every device (MultiDeviceBroadcast)
 *
 * =d
 */
class SetSourceAndOutputForDevice : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  SetSourceAndOutputForDevice();
  ~SetSourceAndOutputForDevice();

  const char *class_name() const  { return "SetSourceAndOutputForDevice"; }
  const char *port_count() const  { return "1/-"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  void push(int, Packet *);

  int initialize(ErrorHandler *);
  void uninitialize();

  void add_handlers();

  void handle_unicast(Packet* p_in);
  void handle_broadcast(Packet* p_in);

 private:
  //
  //member
  //
  NodeIdentity *_id;
  BrnLinkTable *_link_table;
  AssocList *_stations;

  typedef HashMap<EtherAddress, int> EtherAddress2OutputPort;
  EtherAddress2OutputPort _ports;
};

CLICK_ENDDECLS
#endif
