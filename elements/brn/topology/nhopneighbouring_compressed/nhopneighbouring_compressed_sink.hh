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

#ifndef NHOPNEIGHBOURINGCOMPRESSEDSINKELEMENT_HH
#define NHOPNEIGHBOURINGCOMPRESSEDSINKELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/topology/nhopneighbouring/nhopneighbouring_info.hh"

CLICK_DECLS
/*
 * =c
 * NHopNeighbouringCompressedSink()
 * =s
 * Input 0  : broadcast packets
 * =d
 * this is a broadcast-based routing. IT does not implement the broadcast-forwarding itself
 * Unicast-packet from client or brn-node are encaped in broadcast-packets and send using
 * broadcast-flooding, which is an extra element
 */

class NHopNeighbouringCompressedSink : public BRNElement {

 public:

  //
  //methods
  //
  NHopNeighbouringCompressedSink();
  ~NHopNeighbouringCompressedSink();

  const char *class_name() const  { return "NHopNeighbouringCompressedSink"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "1/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //
  NHopNeighbouringInfo *nhop_info;

  Brn2LinkTable *_link_table;

 public:

};

CLICK_ENDDECLS
#endif
