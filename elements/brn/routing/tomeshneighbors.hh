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

#ifndef TOMESHNEIGHBORS_HH
#define TOMESHNEIGHBORS_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "elements/brn/wifi/ap/assoclist.hh"
#include "elements/brn/nodeidentity.hh"

CLICK_DECLS
/*
 * =c
 * ToMeshNeighbors()
 * =s checks if the packet's destination address is a mesh neighbor (a neighbor of my wireless mac address in the LinkTable) or a broadcast (assumes packets are ethernet)
 * output 0: packets for mesh neighbors (including broadcasts)
 * output 1: packets not for mesh neighbors
 * =d
 */
class ToMeshNeighbors : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  ToMeshNeighbors();
  ~ToMeshNeighbors();

  const char *class_name() const	{ return "ToMeshNeighbors"; }
  const char *port_count() const	{ return "1/1-2"; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  void push(int, Packet *);

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //
  class BrnLinkTable *_linktable;
  class NodeIdentity *_id;
};

CLICK_ENDDECLS
#endif
