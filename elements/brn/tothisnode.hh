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

#ifndef TOTHISNODE_HH
#define TOTHISNODE_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "nodeidentity.hh"

CLICK_DECLS
/*
 * =c
 * ToThisNode()
 * =s checks if the packet's destination address is contained in NodeIdentity's list of addresses or a broadcast (assumes packets are ethernet)
 * output 0: packets for this node (including broadcasts)
 * output 1: packets not for this node
 * =d
 */
class ToThisNode : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  ToThisNode();
  ~ToThisNode();

  const char *class_name() const	{ return "ToThisNode"; }
  const char *port_count() const	{ return "1/2"; }
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
  class NodeIdentity *_id;
};

CLICK_ENDDECLS
#endif
