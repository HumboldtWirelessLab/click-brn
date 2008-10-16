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

#ifndef BRNDSELEMENT_HH
#define BRNDSELEMENT_HH

#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include "elements/brn/brn.h"
#include "elements/brn/nodeidentity.hh"
#include "nblist.hh"
#include "elements/brn/routing/linkstat/brnlinktable.hh"

/*
 * =c
 * BRNDS()
 * =s the brn distribution system
 * forwards packet to the right device (wlan0, vlan0, vlan2, ...)
 * packets which could not be forwarded are pushed out on optional output 3
 * =d
 */

#include <click/element.hh>

CLICK_DECLS


class BRNDS : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  BRNDS();
  ~BRNDS();

  const char *class_name() const  { return "BRNDS"; }
  const char *port_count() const  { return "1/3-4"; }
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
  NodeIdentity *_me;
  NeighborList *_nb_lst;
  class BrnLinkTable *_link_table;

  //
  //methods
  //
  void append_hop_to_rreq(Packet *p, EtherAddress *extra_hop);
};

CLICK_ENDDECLS
#endif
