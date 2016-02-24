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

#ifndef BRN2DSRENCAPELEMENT_HH
#define BRN2DSRENCAPELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "brn2_routequerier.hh"
#include "brn2_dsrprotocol.hh"

CLICK_DECLS

/*
 * =c
 * BRN2DSREncap()
 * =s prepends a brn header to a given packet
 * ...
 * =d
 */
class BRN2DSREncap : public BRNElement {

 public:
  //
  //methods
  //
  BRN2DSREncap();
  ~BRN2DSREncap();

  const char *class_name() const	{ return "BRN2DSREncap"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  WritablePacket *add_src_header(Packet *, EtherAddresses);
  Packet *create_rreq(EtherAddress, IPAddress, EtherAddress, IPAddress, uint16_t);
  Packet *create_rrep(EtherAddress, IPAddress, EtherAddress, IPAddress, const BRN2RouteQuerierRoute &, uint16_t rreq_id);
  Packet *create_rerr(EtherAddress, EtherAddress, EtherAddress, const BRN2RouteQuerierRoute &);
  WritablePacket *set_packet_to_next_hop(Packet * p_in);
  WritablePacket *skipInMemoryHops(Packet *p_in);

  //
  //member
  //
  Brn2LinkTable *_link_table;

 private:
  //
  //member
  //
  BRN2NodeIdentity *_me;
};

CLICK_ENDDECLS
#endif
