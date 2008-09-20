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

#ifndef BRNASSOCRESPONDERELEMENT_HH
#define BRNASSOCRESPONDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/handlercall.hh>
#include <elements/wifi/ap/associationresponder.hh>
#include <clicknet/wifi.h>
#include <click/timer.hh>
#include <click/dequeue.hh>
#include "assoclist.hh"

//BRNNEW #include <elements/brn/brnvlan/brnvlan.hh>

CLICK_DECLS

//BRNNEW class BrnIappStationTracker;
class DelayedResponse;

/*
 * =c
 * BRNAssocResponder()
 * =s debugging
 * responds to stations assoc requests
 * =d
 */
class BRNAssocResponder : public AssociationResponder {

  protected:
  
  typedef DEQueue<DelayedResponse*> DelayedResponseQueue;

 public:
  //
  //methods
  //
  BRNAssocResponder();
  ~BRNAssocResponder();

  const char *class_name() const	{ return "BRNAssocResponder"; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  void add_handlers();

  void push(int, Packet *p);

  virtual void recv_association_request(
    Packet *p, uint8_t subtype);
  
  virtual void recv_association_response(
    Packet *p, uint8_t subtype);

  virtual void recv_disassociation(
    Packet *p);

  virtual void send_association_response(
    EtherAddress dst, 
    uint16_t status, 
    uint16_t associd);
  
  virtual void send_reassociation_response(
    EtherAddress dst, 
    uint16_t status, 
    uint16_t associd);
  
  virtual void send_disassociation(
    EtherAddress, 
    uint16_t reason);

  int initialize(ErrorHandler *);

  static void static_response_timer_hook(Timer *, void *);
  void response_timer_hook();

 public:
  //
  //member
  //
  int                         _debug;
  int                         _response_delay_ms;

 private:
//  AssocList*    _client_assoc_lst;
//  String        _device;
//BRNNEW  BrnIappStationTracker*      _iapp;
//BRNNEW  BRNVLAN*                    _brn_vlan;
  Timer                       _response_timer;
  DelayedResponseQueue        _responses;
};

CLICK_ENDDECLS
#endif
