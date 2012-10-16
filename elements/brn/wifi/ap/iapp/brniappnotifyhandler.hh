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

#ifndef BRNIAPP_HH_
#define BRNIAPP_HH_

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>

CLICK_DECLS

class BRN2NodeIdentity;
class BRN2AssocList;
class BrnIappEncap;
class BrnIappStationTracker;
class BRNSetGatewayOnFlow;

/*
 * =c
 * BrnIappNotifyHandler()
 * =s debugging
 * handles the inter-ap protocol type notify within brn
 * =d
 * input 0: brn iapp packets
 * input 1: ether packets to clients
 * input 2: brn iapp peeked packets
 * 
 * @TODO refactor input 3 into an extra element
 */
class BrnIappNotifyHandler : public Element
{
//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
	BrnIappNotifyHandler();
	virtual ~BrnIappNotifyHandler();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const  { return "BrnIappNotifyHandler"; }
  const char *port_count() const  { return "2/2"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  bool can_live_reconfigure() const { return false; }
  void add_handlers();
  
  void run_timer(Timer *);

  void push(int, Packet *);

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------
public:
  void send_handover_notify(
    EtherAddress  client, 
    EtherAddress  apNew, 
    EtherAddress  apOld, 
    uint8_t       seq_no); 

//------------------------------------------------------------------------------
// Internals
//------------------------------------------------------------------------------
protected:
  void send_handover_reply(
    EtherAddress client, 
    EtherAddress apNew, 
    EtherAddress apOld,
    uint8_t      seq_no );

  void recv_handover_notify(
    Packet* p);

  void recv_handover_reply(
    Packet* p);
  
//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  int                      _debug;


  // Elements
  BRN2NodeIdentity*           _id;
  BRN2AssocList*              _assoc_list;
  BrnIappEncap*           _encap;
  BrnIappStationTracker*  _sta_tracker;
  
  class NotifyTimer
  {
    private:
    Timer *_t;
    int _num_notifies;
    Packet *_p;

    public:
    
    NotifyTimer() :
      _t(NULL),
      _num_notifies(0),
      _p(NULL)
    {
    }
    
    NotifyTimer(Timer *t, Packet *p) {
      assert(t != NULL);
      assert(p != NULL);
      _t = t;
      _p = p;
      _num_notifies = 0;
    }
      
    Timer* get_timer() {
      return _t;
    }
    
    Packet* get_packet() {
      return _p;
    }
      
    int get_num_notifies() {
      return _num_notifies;
    }
      
    void inc_num_notifies() {
      ++_num_notifies;
    }
    
    void set_packet(Packet *p) {
      _p = p;
    }
  };
  
  typedef HashMap<EtherAddress, NotifyTimer> NotifyTimers;
  typedef NotifyTimers::iterator NotifyTimersIter;
  
  NotifyTimers _notifytimer;
  #define RESEND_NOTIFY 80 // default to 80ms for resending notify
  uint32_t _notify_ms;
  #define NUM_RESEND 20 // default to 20 times resending
  int _num_resend;
   
  void cleanup_timer_for_client(EtherAddress);
};

CLICK_ENDDECLS
#endif /*BRNIAPP_HH_*/
