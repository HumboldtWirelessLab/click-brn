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

#ifndef NODEIDENTITYELEMENT_HH
#define NODEIDENTITYELEMENT_HH

#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/element.hh>
#include "brn2_device.hh"

CLICK_DECLS

class DeviceInfo;

/*
 * =c
 * BRN2NodeIdentity()
 * =s a list of ethernet addresses
 * stores the ethernet address of associated node (clients, brn nodes) ...
 * =d
 */
class NodeIdentity : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  BRN2NodeIdentity();
  ~BRN2NodeIdentity();

  const char *class_name() const	{ return "NodeIdentity"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  //returns true if the given ethernet address belongs to this node (e.g. wlan-dev)
  bool isIdentical(EtherAddress *);

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //
 Vector<DeviceInfo> _node_devices;

};

CLICK_ENDDECLS
#endif
