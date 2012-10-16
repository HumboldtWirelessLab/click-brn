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

#ifndef BRNIAPPDATAHANDLER_HH_
#define BRNIAPPDATAHANDLER_HH_

#include <click/etheraddress.hh>
#include <click/element.hh>

#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "brniappencap.hh"
#include "brniapprouteupdatehandler.hh"

CLICK_DECLS

//class BRN2NodeIdentity;
//class RBN2AssocList;
//class BrnIappEncap;
//class BrnIappRouteUpdateHandler;

/*
 * =c
 * BrnIapp()
 * =s debugging
 * handles the inter-ap protocol within brn
 * =d
 * input 0: brn iapp packets type data
 * input 0: ether packets 
 */
class BrnIappDataHandler : public Element
{
//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
	BrnIappDataHandler();
	virtual ~BrnIappDataHandler();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const  { return "BrnIappDataHandler"; }
  const char *port_count() const  { return "2/1-2"; }
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
  void handle_handover_data(
    EtherAddress sta, 
    EtherAddress ap_new, 
    EtherAddress ap_old,
    uint8_t      seq_nr,
    Packet*      p );

protected:
  void send_handover_data(
    EtherAddress dst, 
    EtherAddress src, 
    EtherAddress client, 
    EtherAddress apNew, 
    EtherAddress apOld,
    Packet*      p ); 

  void recv_handover_data(
    Packet* p);

  void recv_ether(Packet* p);

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  int                        _debug;
  bool                       _optimize;

  // Elements
  BRN2NodeIdentity*           _id;
  BRN2AssocList*              _assoc_list;
  BrnIappEncap*               _encap;
  BrnIappRouteUpdateHandler*  _route_handler;
};

CLICK_ENDDECLS
#endif /*BRNIAPPDATAHANDLER_HH_*/
