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

#ifndef SRCFORWARDERELEMENT_HH
#define SRCFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "elements/brn/nodeidentity.hh"
#include "elements/brn/wifi/ap/assoclist.hh"
#include "dsrencap.hh"
#include "dsrdecap.hh"
#include "routequerier.hh"

CLICK_DECLS

/*
 * =c
 * SrcForwarder()
 * =s forwards src packets
 * forwards dsr source routed packets
 * =d
 */
class SrcForwarder : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  SrcForwarder();
  ~SrcForwarder();

  const char *class_name() const  { return "SrcForwarder"; }
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
  NodeIdentity *_me;
  AssocList *_client_assoc_lst;
  DSREncap *_dsr_encap;
  DSRDecap *_dsr_decap;
  BrnLinkTable *_link_table;

  //
  //methods
  //
  void forward_data(Packet *p_in);
  Packet *strip_all_headers(Packet *p_in);
  void add_route_to_link_table(const RouteQuerierRoute &route);
};

CLICK_ENDDECLS
#endif
