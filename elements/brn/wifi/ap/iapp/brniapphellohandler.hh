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

#ifndef BRNIAPPHELLOSOURCE_HH_
#define BRNIAPPHELLOSOURCE_HH_

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/timer.hh>
#include <click/deque.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"

CLICK_DECLS

class BRN2AssocList;
class BrnIappEncap;

/*
 * =c
 * BrnIappHelloHandler()
 * =s debugging
 * generates and handles hello packets 
 * =d
 * input 0: brn iapp hello packets for this node
 */
class BrnIappHelloHandler : public BRNElement
{
//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
  typedef Deque<EtherAddress> EtherAddressQueue;

//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
	BrnIappHelloHandler();
	virtual ~BrnIappHelloHandler();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const  { return "BrnIappHelloHandler"; }
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
  void send_iapp_hello(
    EtherAddress sta,
    EtherAddress ap_cand, 
    EtherAddress ap_curr,
    bool         to_curr);

  void schedule_hello(
    EtherAddress sta);
  
//------------------------------------------------------------------------------
// Internals
//------------------------------------------------------------------------------
protected:

  void hello_trigger();

  void send_hello();

  static void static_timer_trigger(Timer *, void *);
  static void static_timer_hello_trigger(Timer *, void *);

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  bool              _optimize;
  EtherAddressQueue _sta_hello_queue;

  Timer             _timer;
  Timer             _timer_hello;
  static int        _hello_trigger_interval_ms;

  // Elements
  BRN2NodeIdentity*     _id;
  BRN2AssocList*        _assoc_list;
  Brn2LinkTable*     _link_table;
  BrnIappEncap*     _encap;
};

CLICK_ENDDECLS
#endif /*BRNIAPPHELLOSOURCE_HH_*/
