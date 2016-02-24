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

#ifndef BRNIAPPENCAP_HH_
#define BRNIAPPENCAP_HH_

#include <click/etheraddress.hh>
#include <click/element.hh>

CLICK_DECLS

/*
 * =c
 * BrnIappEncap()
 * =s debugging
 * encapsulation support for the inter-ap protocol
 * =d
 * 
 * @TODO refactor input 3 into an extra element
 */
class BrnIappEncap : public Element
{
//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
	BrnIappEncap();
	virtual ~BrnIappEncap();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const  { return "BrnIappEncap"; }
  const char *port_count() const  { return "0/0"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  bool can_live_reconfigure() const { return false; }
  void add_handlers();

  //Packet* simple_action(Packet*) {}

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------
public:
  WritablePacket* create_handover_notify(
    EtherAddress  client, 
    EtherAddress  apNew, 
    EtherAddress  apOld, 
    uint8_t       seq_no); 

  Packet* create_handover_reply(
    EtherAddress client, 
    EtherAddress apNew, 
    EtherAddress apOld,
    uint8_t      seq_no );

  Packet* create_handover_data(
    EtherAddress dst, 
    EtherAddress src, 
    EtherAddress client, 
    EtherAddress apNew, 
    EtherAddress apOld,
    Packet*      p ); 

  Packet* create_handover_routeupdate(
    EtherAddress dst,
    EtherAddress src,
    EtherAddress sta,
    EtherAddress ap_new, 
    EtherAddress ap_old,
    uint8_t      seq_no);

  Packet* create_iapp_hello(
    EtherAddress sta,
    EtherAddress ap_cand, 
    EtherAddress ap_curr,
    bool         to_curr);

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  int              _debug;
};

CLICK_ENDDECLS
#endif /*BRNIAPPENCAP_HH_*/
