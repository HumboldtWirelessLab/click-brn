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
#include "elements/brn/common.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

CLICK_DECLS

class RouteQuerierHop;

typedef Vector<RouteQuerierHop> BRN2RouteQuerierRoute;


/*
 * =c
 * BRN2DSRDecap()
 * =s removes the dsr header from packets
 * ...
 * =d
 */
class BRN2DSRDecap : public Element {

 public:
  //
  //member
  //
  int _debug;
  Brn2LinkTable *_link_table;

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

  void extract_request_route(const Packet *, int *ref_metric, BRN2RouteQuerierRoute &route);
  void extract_reply_route(const Packet *p, BRN2RouteQuerierRoute &route);
  void extract_source_route(const Packet *p_in, BRN2RouteQuerierRoute &route);

 public:
  //
  //member
  //
  class BRN2NodeIdentity *_me;
};

CLICK_ENDDECLS
#endif