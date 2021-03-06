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

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

#include "elements/brn/routing/broadcast/flooding/flooding_db.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_helper.hh"

#include "elements/brn/routing/broadcast/flooding/passiveack/passiveackpacketinfo.hh"
#include "elements/brn/routing/broadcast/flooding/passiveack/floodingtxscheduling.hh"

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

#define PASSIVE_ACK_DFL_MAX_RETRIES  1
#define PASSIVE_ACK_DFL_INTERVAL    25
#define PASSIVE_ACK_DFL_TIMEOUT   2000

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

public:
  FloodingHelper *_fhelper;
  FloodingDB *_flooding_db;
  FloodingTxScheduling *_flooding_scheduling;

private:
  PAckPacketVector p_queue;

  uint32_t _dfl_retries;
  uint32_t _dfl_timeout;

  uint32_t _cntbased_min_neighbors_for_abort;
  bool _abort_on_finished;

  bool packet_is_finished(PassiveAckPacket *pap);

  uint32_t _enqueued_pkts, _queued_pkts, _dequeued_pkts, _retransmissions, _pre_removed_pkts, _pre_removed_pkts_timeout;

  PassiveAckPacket *get_pap(EtherAddress *src, uint16_t bcast_id) const;

  Vector<EtherAddress>* get_passive_ack_neighbors(PassiveAckPacket *pap);
  int set_unfinished_neighbors(PassiveAckPacket *pap);
  int count_unfinished_neighbors(PassiveAckPacket *pap);

 public:

  int (*_retransmit_broadcast)(BRNElement *e, Packet *, EtherAddress *, uint16_t);

  void set_retransmit_bcast(BRNElement *e, int (*retransmit_bcast)(BRNElement *e, Packet *, EtherAddress *, uint16_t)) {
    _retransmit_element = e;
    _retransmit_broadcast = retransmit_bcast;
  }

  int packet_enqueue(Packet *p, EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries);

  void packet_dequeue(EtherAddress *src, uint16_t bcast_id);

  void handle_feedback_packet(Packet *p, EtherAddress *src, uint16_t bcast_id, bool rejected, bool abort, uint8_t no_transmissions);

  void update_flooding_info(Packet *p, EtherAddress &src, uint16_t bcast_id);

  int tx_delay(PassiveAckPacket *pap);

  String stats();

};

CLICK_ENDDECLS
#endif
