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

#ifndef BRN2ASSOCRESPONDERELEMENT_HH
#define BRN2ASSOCRESPONDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/handlercall.hh>
#include <clicknet/wifi.h>
#include <click/timer.hh>
#include <click/deque.hh>
#include <elements/wifi/ap/associationresponder.hh>
#include "brn2_assoclist.hh"
#include "../brn2_wirelessinfolist.hh"
#include "../../brnprotocol/brnpacketanno.hh"
#include "../../routing/identity/brn2_device.hh"
#include "elements/brn/vlan/brn2vlantable.hh"

CLICK_DECLS

class DelayedResponse;

/*
 * =c
 * BRN2AssocResponder()
 * =s debugging
 * responds to stations assoc requests
 * =d
 */
class BRN2AssocResponder : public AssociationResponder {

  protected:

  typedef Deque<DelayedResponse*> DelayedResponseQueue;

 public:
  //
  //methods
  //
  BRN2AssocResponder();
  ~BRN2AssocResponder();

  const char *class_name() const	{ return "BRN2AssocResponder"; }
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
  Timer                       _response_timer;
  DelayedResponseQueue        _responses;
  BRN2AssocList               *_assoc_list;
  BRN2Device                  *_dev;
  BRN2WirelessInfoList        *_winfolist;
  BRN2VLANTable               *_vlantable;
  class BrnAvailableRates     *_rtable;

};

CLICK_ENDDECLS
#endif
