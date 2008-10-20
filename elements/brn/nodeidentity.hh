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
#include "elements/brn/routing/linkstat/brnlinktable.hh"
#include "elements/brn/routing/dsr/routequerier.hh"

CLICK_DECLS

class BrnLinkTable;

/*
 * =c
 * NodeIdentity()
 * =s a list of ethernet addresses
 * stores the ethernet address of associated node (clients, brn nodes) ...
 * =d
 */
class NodeIdentity : public Element {

#define DEVICE_WLAN     "wlan0"
#define DEVICE_VLAN0    "vlan0"
#define DEVICE_VLAN1    "vlan1"

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  NodeIdentity();
  ~NodeIdentity();

  const char *class_name() const	{ return "NodeIdentity"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  //returns true if the given ethernet address belongs to this node (e.g. wlan-dev)
  bool isIdentical(EtherAddress *);

  EtherAddress *getMyWirelessAddress();
  EtherAddress *getMyVlan0Address();
  EtherAddress *getMyVlan1Address();

  int findOwnIdentity(const RouteQuerierRoute &r);

  EtherAddress *getEthernetAddress(String);

  //this method skips inmemory hops (used for scr and rrep packets)
  Packet *skipInMemoryHops(Packet *p_in);

  const String& getWlan0DeviceName();
  const String& getVlan0DeviceName();
  const String& getVlan1DeviceName();
  
  BrnLinkTable* get_link_table();

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //
  String _dev_wlan_name;
  EtherAddress *_me_wlan;
  String _dev_vlan0_name;
  EtherAddress *_me_vlan0;
  String _dev_vlan1_name;
  EtherAddress *_me_vlan1;

  BrnLinkTable *_link_table;
};

CLICK_ENDDECLS
#endif
