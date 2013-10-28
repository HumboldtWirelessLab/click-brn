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
      EtherAddress _src;
      uint16_t _bcast_id;
      Vector<EtherAddress> _passiveack;

      int16_t _max_retries;
      uint16_t _retries;

      Timestamp _enqueue_time;
      Timestamp _last_tx;

      PassiveAckPacket(EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries)
      {
        _src = EtherAddress(src->data());
        _bcast_id = bcast_id;
        if ( passiveack != NULL )
        for ( int i = 0; i < passiveack->size(); i++) _passiveack.push_back((*passiveack)[i]);

        _max_retries = retries;

        _last_tx = _enqueue_time = Timestamp::now();
        _retries = 0;
      }

      ~PassiveAckPacket() {
        _passiveack.clear();
      }

      void set_tx(Timestamp tx_time) {
        _last_tx = tx_time;
      }

      inline void inc_max_retries() { _max_retries++; }; //use for tx_abort (is an net_layer transmission but maybe no mac-layer tx)

      void set_next_retry(Timestamp tx_time) {
        _retries++;
        _last_tx = tx_time;
      }

      inline int32_t tx_duration(Timestamp now) {
        return (now - _last_tx).msecval();
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

  bool packet_is_finished(PassiveAckPacket *pap);

  uint32_t _enqueued_pkts, _queued_pkts, _dequeued_pkts, _retransmissions, _pre_removed_pkts;

  int count_unfinished_neighbors(PassiveAckPacket *pap);

 public:

  int (*_retransmit_broadcast)(BRNElement *e, Packet *, EtherAddress *, uint16_t);

  void set_retransmit_bcast(BRNElement *e, int (*retransmit_bcast)(BRNElement *e, Packet *, EtherAddress *, uint16_t)) {
    _retransmit_element = e;
    _retransmit_broadcast = retransmit_bcast;
  }

  void set_flooding(Flooding *flooding) { _flooding = flooding; }

  int packet_enqueue(Packet *p, EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries);

  void packet_dequeue(EtherAddress *src, uint16_t bcast_id);

  void handle_feedback_packet(Packet *p, EtherAddress *src, uint16_t bcast_id, bool rejected, bool abort, uint8_t no_transmissions);

  int tx_delay(PassiveAckPacket *pap);

  String stats();

};

CLICK_ENDDECLS
#endif
