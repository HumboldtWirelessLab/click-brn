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

#ifndef REPLYFORWARDERELEMENT_HH
#define REPLYFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "dsrdecap.hh"
#include "reqforwarder.hh"
#include "nodeidentity.hh"

CLICK_DECLS

/*
 * =c
 * ReplyForwarder()
 * =s forwards rrep packets
 * forwards dsr route reply packets
 * =d
 */
class ReplyForwarder : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  ReplyForwarder();
  ~ReplyForwarder();

  const char *class_name() const  { return "ReplyForwarder"; }
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
  DSREncap *_dsr_encap;
  DSRDecap *_dsr_decap;
  RouteQuerier *_route_querier;
  AssocList *_client_assoc_lst;
  BrnLinkTable *_link_table;

  //
  //methods
  //
  void forward_rrep(Packet *);
  void add_route_to_link_table(const RouteQuerierRoute &route, uint32_t _end_index); //learn route up to end_index (route_prefix)
};

CLICK_ENDDECLS
#endif
