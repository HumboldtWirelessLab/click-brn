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
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/wifi/ap/brn2_assoclist.hh"
#include "brn2_dsrencap.hh"
#include "brn2_dsrdecap.hh"
#include "brn2_routequerier.hh"

CLICK_DECLS

/*
 * =c
 * BRN2SrcForwarder()
 * =s forwards src packets
 * forwards dsr source routed packets
 * =d
 */
class BRN2SrcForwarder : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  BRN2SrcForwarder();
  ~BRN2SrcForwarder();

  const char *class_name() const  { return "BRN2SrcForwarder"; }
  const char *port_count() const  { return "2/3"; }
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

  //
  //methods
  //
  void forward_data(Packet *p_in);
  Packet *strip_all_headers(Packet *p_in);
  void add_route_to_link_table(const BRN2RouteQuerierRoute &route);
  Packet *skipInMemoryHops(Packet *p_in);
};

CLICK_ENDDECLS
#endif
