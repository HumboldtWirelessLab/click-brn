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

#ifndef DSRENCAPELEMENT_HH
#define DSRENCAPELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "routequerier.hh"

CLICK_DECLS

typedef Vector<RouteQuerierHop> RouteQuerierRoute;
typedef Vector<EtherAddress> EtherAddresses;



/*
 * =c
 * DSREncap()
 * =s prepends a brn header to a given packet
 * ...
 * =d
 */
class DSREncap : public Element {

 public:
  //
  //member
  //
  int _debug;
  BrnLinkTable *_link_table;

  //
  //methods
  //
  DSREncap();
  ~DSREncap();

  const char *class_name() const	{ return "DSREncap"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  Packet *add_src_header(Packet *, EtherAddresses);
  Packet *create_rreq(EtherAddress, IPAddress, EtherAddress, IPAddress, uint16_t);
  Packet *create_rrep(EtherAddress, IPAddress, EtherAddress, IPAddress, const RouteQuerierRoute &, uint16_t rreq_id);
  Packet *create_rerr(EtherAddress, EtherAddress, EtherAddress, const RouteQuerierRoute &);
  Packet *set_packet_to_next_hop(Packet * p_in);

 private:
  //
  //member
  //
  class NodeIdentity *_me;
};

CLICK_ENDDECLS
#endif