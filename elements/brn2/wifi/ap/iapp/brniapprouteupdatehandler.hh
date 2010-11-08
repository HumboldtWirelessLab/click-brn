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

#ifndef BRNIAPPROUTEUPDATEHANDLER_HH_
#define BRNIAPPROUTEUPDATEHANDLER_HH_

#include <click/etheraddress.hh>
#include <click/element.hh>

CLICK_DECLS

class BrnIappEncap;
class BRN2AssocList;
class BrnIappHelloHandler;
class BRN2NodeIdentity;
class Brn2LinkTable;

/*
 * =c
 * BrnIapp()
 * =s debugging
 * handles the inter-ap protocol type route update within brn
 * =d
 * 
 */
class BrnIappRouteUpdateHandler : public Element
{
//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
	BrnIappRouteUpdateHandler();
	virtual ~BrnIappRouteUpdateHandler();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const  { return "BrnIappRouteUpdateHandler"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  bool can_live_reconfigure() const { return false; }
  void add_handlers();

  void push(int, Packet *);

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------
public:
  void send_handover_routeupdate(
    EtherAddress dst,
    EtherAddress src,
    EtherAddress sta,
    EtherAddress ap_new, 
    EtherAddress ap_old,
    uint8_t      seq_no);

protected:
  void recv_handover_routeupdate(
    Packet* p);

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  int                   _debug;

  BRN2NodeIdentity*     _id;
  BRN2AssocList*        _assoc_list;
  Brn2LinkTable*        _link_table;
  BrnIappEncap*         _encap;
  BrnIappHelloHandler*  _hello_handler;
};

CLICK_ENDDECLS
#endif /*BRNIAPPROUTEUPDATEHANDLER_HH_*/
