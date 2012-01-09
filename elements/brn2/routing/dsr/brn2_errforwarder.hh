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

#ifndef BRN2ERRFORWARDERELEMENT_HH
#define BRN2ERRFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "brn2_dsrencap.hh"
#include "brn2_dsrdecap.hh"

CLICK_DECLS

/*
 * =c
 * BRN2ErrorForwarder()
 * =s forwards rerr packets
 * forwards dsr route error packets
 * =d
 */
class BRN2ErrorForwarder : public BRNElement {

 #define BRN_DSR_ROUTE_ERR_NEIGHBORS_PUNISHMENT_FAC 4

 public:
  //
  //methods
  //
  BRN2ErrorForwarder();
  ~BRN2ErrorForwarder();

  const char *class_name() const  { return "BRN2ErrorForwarder"; }
  const char *port_count() const  { return "2/1"; }
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
  Brn2LinkTable *_link_table;
  BRN2DSREncap *_dsr_encap;
  BRN2DSRDecap *_dsr_decap;
  BRN2RouteQuerier *_route_querier;

  //
  //methods
  //
  void forward_rerr(Packet *p_in);
  void issue_rerr(EtherAddress, EtherAddress, EtherAddress, const BRN2RouteQuerierRoute &);
  void truncate_route(const BRN2RouteQuerierRoute &r, EtherAddress bad_src, BRN2RouteQuerierRoute &rv);
  void reverse_route(const BRN2RouteQuerierRoute &in, BRN2RouteQuerierRoute &out); //TODO extract to common
};

CLICK_ENDDECLS
#endif
