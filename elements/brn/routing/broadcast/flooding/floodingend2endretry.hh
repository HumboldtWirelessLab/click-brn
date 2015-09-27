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

#ifndef FLOODINGEND2ENDRETRYELEMENT_HH
#define FLOODINGEND2ENDRETRYELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

/*
 * =c
 * FloodingEnd2EndRetry()
 * =s
 *
 * =d
 */

class FloodingEnd2EndRetry : public BRNElement {

 public:
  class RetryPacket
  {
     public:
      Packet *_p;
      int16_t _retries;
      int16_t _max_retries; 
      
      Timestamp _enqueue_time;
      
      uint32_t _timeout;       //delay between
      Timestamp _last_timeout;
      Timestamp _next_timeout;
      
      RetryPacket(Packet *p, int16_t retries, uint32_t timeout)
      {
        _p = p;
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
   
   typedef Vector<RetryPacket*> RetryPacketVector;
   typedef RetryPacketVector::const_iterator RetryPacketVectorIter;
  //
  //methods
  //
  FloodingEnd2EndRetry();
  ~FloodingEnd2EndRetry();

  const char *class_name() const  { return "FloodingEnd2EndRetry"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void run_timer(Timer *);
  
  void add_handlers();
  
  void push( int port, Packet *packet );

 private:
  //
  //member
  RetryPacketVector p_queue;
  
  uint32_t _dfl_retries;
  uint32_t _dfl_timeout;

  uint32_t _time_tolerance;
  
  Timer _retransmit_timer;
  
  uint32_t _queued_pkts;
  //uint32_t _dequeued_pkts;
  //uint32_t _retransmissions;

 public:
    
  String stats();

};

CLICK_ENDDECLS
#endif
