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

#ifndef BRN2DSRDECAPELEMENT_HH
#define BRN2DSRDECAPELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

#include "brn2_dsrprotocol.hh"

CLICK_DECLS


/*
 * =c
 * BRN2DSRDecap()
 * =s removes the dsr header from packets
 * ...
 * =d
 */
class BRN2DSRDecap : public BRNElement {

 public:
  //
  //methods
  //
  BRN2DSRDecap();
  ~BRN2DSRDecap();

  const char *class_name() const	{ return "BRN2DSRDecap"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  //return metric of hole route
  uint16_t extract_request_route(const Packet *, BRN2RouteQuerierRoute &route,
                             EtherAddress *last_hop, uint16_t last_hop_metric);
  void extract_reply_route(const Packet *p, BRN2RouteQuerierRoute &route);
  void extract_source_route(const Packet *p_in, BRN2RouteQuerierRoute &route);

};

CLICK_ENDDECLS
#endif
