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

#ifndef FLOODINGPASSIVEACKELEMENT_HH
#define FLOODINGPASSIVEACKELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>
#include <click/timer.hh>

#include "elements/brn/routing/broadcast/flooding/flooding_helper.hh"
#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

CLICK_DECLS

/*
 * =c
 * FloodingPassiveAck()
 * =s
 *
 * =d
 */
 
 /*
  * Flooding (oder ein andere element) 
  * 
  * 
  * 
  * 
  * 
  */

class Flooding;
 
class FloodingPassiveAck : public BRNElement {

 public:
  class PassiveAckPacket
  {
#define PASSIVE_ACK_DFL_MAX_RETRIES 10
     public:
      Packet *_p;
      EtherAddress _src;
      uint16_t _bcast_id;
      Vector<EtherAddress> _passiveack;
      
      int16_t _max_retries;
      uint16_t _retries;
      
      Timestamp _enqueue_time;
      
      uint32_t _timeout;
      Timestamp _last_timeout;
      Timestamp _next_timeout;
      
      PassiveAckPacket(Packet *p, EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries, uint32_t timeout)
      {
        _p = p;
	_src = EtherAddress(src->data());
	_bcast_id = bcast_id;
	if ( passiveack != NULL )
	  for ( int i = 0; i < passiveack->size(); i++) _passiveack.push_back((*passiveack)[i]);
	 
        _max_retries = retries;
	
	_last_timeout = _enqueue_time = Timestamp::now();
	_retries = 0;
	set_timeout(timeout);
      }

      void set_timeout(uint32_t timeout) {
      	_timeout = timeout;
	_next_timeout = _last_timeout + Timestamp::make_msec(timeout);
      }

      void set_next_timeout() {
	_last_timeout = _next_timeout;
	_next_timeout = _last_timeout + Timestamp::make_msec(_timeout);
      }

      void set_next_retry() {
	_retries++;
	set_next_timeout();
      }
      
      inline int32_t time_left(Timestamp now) {
	return (_next_timeout - now).msecval();
      }
      
      inline uint32_t retries_left() {
	return _max_retries - _retries;
      }

   };
   
   typedef Vector<PassiveAckPacket*> PAckPacketVector;
   typedef PAckPacketVector::const_iterator PAckPacketVectorIter;
  //
  //methods
  //
  FloodingPassiveAck();
  ~FloodingPassiveAck();

  const char *class_name() const  { return "FloodingPassiveAck"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void run_timer(Timer *);
  
  void add_handlers();

 private:
  //
  //member
  //
  BRN2NodeIdentity *_me;
  BRNElement *_retransmit_element;  
  Flooding *_flooding;
  FloodingHelper *_fhelper;

  PAckPacketVector p_queue;
  
  uint32_t _dfl_retries;
  uint32_t _dfl_timeout;

  bool _enable;
  bool _queue_check;
  uint32_t _time_tolerance;

  PassiveAckPacket *get_next_packet();
  void set_next_schedule();
  void scan_packet_queue(int32_t time_tolerance);
  bool has_packet_in_queue(PassiveAckPacket *pap);
  bool packet_is_finished(PassiveAckPacket *pap);
  
  Timer _retransmit_timer;
  bool _timer_is_scheduled;
  Timestamp _time_next_schedule;
  
  uint32_t _queued_pkts;
  uint32_t _dequeued_pkts;
  uint32_t _retransmissions;
  uint32_t _pre_removed_pkts;
  uint32_t _already_queued_pkts;

 public:
  
  void enable(bool e) { _enable = e; };

  int (*_retransmit_broadcast)(BRNElement *e, Packet *, EtherAddress *, uint16_t);

  void set_retransmit_bcast(BRNElement *e, int (*retransmit_bcast)(BRNElement *e, Packet *, EtherAddress *, uint16_t)) {
    _retransmit_element = e;
    _retransmit_broadcast = retransmit_bcast;
  }

  void set_flooding(Flooding *flooding) { _flooding = flooding; }
  
  int packet_enqueue(Packet *p, EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries);

  void packet_dequeue(EtherAddress *src, uint16_t bcast_id);
    
  String stats();

};

CLICK_ENDDECLS
#endif
