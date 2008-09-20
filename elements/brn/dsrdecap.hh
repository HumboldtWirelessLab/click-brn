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

#ifndef DSRDECAPELEMENT_HH
#define DSRDECAPELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "common.hh"

CLICK_DECLS

class RouteQuerierHop;

typedef Vector<RouteQuerierHop> RouteQuerierRoute;


/*
 * =c
 * DSRDecap()
 * =s removes the dsr header from packets
 * ...
 * =d
 */
class DSRDecap : public Element {

 public:
  //
  //member
  //
  int _debug;
  BrnLinkTable *_link_table;

  //
  //methods
  //
  DSRDecap();
  ~DSRDecap();

  const char *class_name() const	{ return "DSRDecap"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void extract_request_route(const Packet *, int *ref_metric, RouteQuerierRoute &route);
  void extract_reply_route(const Packet *p, RouteQuerierRoute &route);
  void extract_source_route(const Packet *p_in, RouteQuerierRoute &route);

 public:
  //
  //member
  //
  class NodeIdentity *_me;
};

CLICK_ENDDECLS
#endif
