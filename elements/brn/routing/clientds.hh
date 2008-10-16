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

#ifndef CLIENTDSELEMENT_HH
#define CLIENTDSELEMENT_HH

#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include "elements/brn/brn.h"
#include "elements/brn/wifi/ap/assoclist.hh"

/*
 * =c
 * ClientDS()
 * =s the client distribution system
 * forwards client packets to the right device (wlan0, vlan0, vlan2, ...)
 * =d
 */

#include <click/element.hh>

CLICK_DECLS

class ClientDS : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  ClientDS();
  ~ClientDS();

  const char *class_name() const  { return "ClientDS"; }
  const char *port_count() const  { return "1/4"; }
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
  AssocList *_client_assoc_lst;
  String _dev_wlan_name;
  String _dev_vlan1_name;
  String _dev_vlan2_name;
  String _dev_vlan_debug_name;
};

CLICK_ENDDECLS
#endif
