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

#ifndef ERRFORWARDERELEMENT_HH
#define ERRFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "common.hh"
#include "assoclist.hh"
#include "nodeidentity.hh"

CLICK_DECLS

/*
 * =c
 * ErrorForwarder()
 * =s forwards rerr packets
 * forwards dsr route error packets
 * =d
 */
class ErrorForwarder : public Element {

 #define BRN_DSR_ROUTE_ERR_NEIGHBORS_PUNISHMENT_FAC 4

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  ErrorForwarder();
  ~ErrorForwarder();

  const char *class_name() const  { return "ErrorForwarder"; }
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
  NodeIdentity *_me;
  BrnLinkTable *_link_table;
  AssocList *_client_assoc_lst;
  DSREncap *_dsr_encap;
  DSRDecap *_dsr_decap;
  RouteQuerier *_route_querier;
  BRNEncap *_brn_encap;

  //
  //methods
  //
  void forward_rerr(Packet *p_in);
  void issue_rerr(EtherAddress, EtherAddress, EtherAddress, const RouteQuerierRoute &);
  void truncate_route(const RouteQuerierRoute &r, EtherAddress bad_src, RouteQuerierRoute &rv);
  void reverse_route(const RouteQuerierRoute &in, RouteQuerierRoute &out); //TODO extract to common
};

CLICK_ENDDECLS
#endif
