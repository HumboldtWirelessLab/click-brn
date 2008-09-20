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

#ifndef ONEHOPNEIGHBORELEMENT_HH
#define ONEHOPNEIGHBORELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "assoclist.hh"
#include "nodeidentity.hh"

CLICK_DECLS
/*
 * =c
 * OneHopNeighbor()
 * =s checks if destination is 1-hop-reachable
 * checks if the destination of a given non-brn packet could by reached with one hop
 * output 0: destination (client) is associated with me
 * output 1: packet has to be forwarded (over internal nodes)
 * output 2: packet is for me
 * =d
 */
class OneHopNeighbor : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  OneHopNeighbor();
  ~OneHopNeighbor();

  const char *class_name() const	{ return "OneHopNeighbor"; }
  const char *port_count() const	{ return "1/3"; }
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
  class NodeIdentity *_me;
  class AssocList *_client_assoc_lst;
};

CLICK_ENDDECLS
#endif
