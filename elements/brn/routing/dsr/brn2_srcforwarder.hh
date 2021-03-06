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

#ifndef BRN2SRCFORWARDERELEMENT_HH
#define BRN2SRCFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "brn2_dsrencap.hh"
#include "brn2_dsrdecap.hh"
#include "brn2_routequerier.hh"
#include "brn2_routeidcache.hh"

CLICK_DECLS

/*
 * =c
 * BRN2SrcForwarder()
 * =s forwards src packets
 * forwards dsr source routed packets
 * =d
 */

/* PORTS
 * Input:
 * 0 local
 * 1 from other nodes to this nodes
 * 2 from other nodes (overhear)
 * 
 * Output:
 * 0 local
 * 1 for next brn node
 * 2 route error
 * 
 */
class BRN2SrcForwarder : public BRNElement {

 public:
  //
  //methods
  //
  BRN2SrcForwarder();
  ~BRN2SrcForwarder();

  const char *class_name() const  { return "BRN2SrcForwarder"; }
  const char *port_count() const  { return "2-3/3"; }
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
  BRN2NodeIdentity *_me;
  BRN2DSREncap *_dsr_encap;
  BRN2DSRDecap *_dsr_decap;
  Brn2LinkTable *_link_table;
  BRN2RouteQuerier *_route_querier;
  BrnRouteIdCache *_dsr_rid_cache;

  //
  //methods
  //
  void forward_data(Packet *p_in);
  Packet *skipInMemoryHops(Packet *p_in);

 public:
  static Packet *strip_all_headers(Packet *p_in);
};

CLICK_ENDDECLS
#endif
