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

#ifndef FLOODINGELEMENT_HH
#define FLOODINGELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/timestamp.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

#include "flooding_helper.hh"
#include "flooding_db.hh"
#include "floodingpolicy/floodingpolicy.hh"
#include "floodingpassiveack.hh"

CLICK_DECLS

/**  TODO:
 * forward a known packet again? ask   policy if the packet is known ?
 */
/*
 * !!! BROADCAST ID -> 0 is never used !!! Don't use 0 as bcastid !!
 */

struct click_brn_bcast {
  uint16_t      bcast_id;
  uint8_t       flags;
  uint8_t       extra_data_size;
} CLICK_SIZE_PACKED_ATTRIBUTE ;

#define BCAST_HEADER_FLAGS_FORCE_DST           1 /* src is responsible for target */
#define BCAST_HEADER_FLAGS_REJECT_ON_EMPTY_CS  2 /* reason for reject was empty cs */
#define BCAST_HEADER_FLAGS_REJECT_WITH_ASSIGN  4 /* reason for reject was empty cs with assigned nodes*/
#define BCAST_HEADER_FLAGS_REJECT_DUE_STOPPED  8 /* reason for reject was the stopped transmission*/

#define BCAST_MAX_EXTRA_DATA_SIZE            255

struct click_brn_bcast_extra_data {
  uint8_t size;
  uint8_t type;
} CLICK_SIZE_PACKED_ATTRIBUTE ;

#define BCAST_EXTRA_DATA_MPR                    1
#define BCAST_EXTRA_DATA_NODEINFO               2

/*
 * =c
 * Flooding()
 * =s
 * Input 0  : Packets to route
 * Input 1  : BRNFlooding-Packets
 * Input 2  : TXFeedback failure
 * Input 3  : TXFeedback success
 * Input 4  : Passive (Foreign)
 * 
 * Output 0 : Packets to local
 * Output 1 : BRNBroadcastRouting-Packets
 * =d
 */

class Flooding : public BRNElement {

 public:

  //
  //methods
  //
  Flooding();
  ~Flooding();

  const char *class_name() const  { return "Flooding"; }
  const char *processing() const  { return PUSH; }

/**
 * 0: broadcast
 * 1: brn
 * 2: txfeedback failure
 * 3: txfeedback success
 * 4: passive overhear
 * 5: low layer reject
 **/

  const char *port_count() const  { return "2-6/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  int retransmit_broadcast(Packet *p, EtherAddress *src, uint16_t bcast_id);

  String stats();
  String table();
  void reset();

 private:
  //
  //member
  //
  BRN2NodeIdentity *_me;

  FloodingHelper *_fhelper;
  FloodingDB *_flooding_db;
  FloodingPolicy *_flooding_policy;
  FloodingPassiveAck *_flooding_passiveack;

  uint16_t _bcast_id;

  uint8_t extra_data[BCAST_MAX_EXTRA_DATA_SIZE];
  uint32_t extra_data_size;

  bool _passive_last_node_new;
  bool _passive_last_node_rx_acked;
  bool _passive_last_node_assign;
  bool _passive_last_node_foreign_responsibility;

 public:

  bool is_local_addr(EtherAddress *ea) { return _me->isIdentical(ea);}

  uint32_t _flooding_src;
  uint32_t _flooding_rx;
  uint32_t _flooding_sent;
  uint32_t _flooding_fwd;
  uint32_t _flooding_passive;
  uint32_t _flooding_passive_not_acked_dst;
  uint32_t _flooding_passive_not_acked_force_dst;
  uint32_t _flooding_node_info_due_to_passive;
  uint32_t _flooding_node_info_new_finished;
  uint32_t _flooding_node_info_due_to_piggyback;
  uint32_t _flooding_lower_layer_reject;
  uint32_t _flooding_src_new_id;
  uint32_t _flooding_rx_new_id;
  uint32_t _flooding_fwd_new_id;
  uint32_t _flooding_rx_ack;


  /* Members and functions for tx abort */
#define FLOODING_TXABORT_MODE_NONE         0
#define FLOODING_TXABORT_MODE_ACKED        1
#define FLOODING_TXABORT_MODE_ASSIGNED     2
#define FLOODING_TXABORT_MODE_BETTER_LINK  4
#define FLOODING_TXABORT_MODE_NEW_INFO     8
#define FLOODING_TXABORT_MODE_FINISHED    16

#define FLOODING_TXABORT_REASON_NONE                    FLOODING_TXABORT_MODE_NONE
#define FLOODING_TXABORT_REASON_ACKED                   FLOODING_TXABORT_MODE_ACKED
#define FLOODING_TXABORT_REASON_ASSIGNED                FLOODING_TXABORT_MODE_ASSIGNED
#define FLOODING_TXABORT_REASON_BETTER_LINK             FLOODING_TXABORT_MODE_BETTER_LINK
#define FLOODING_TXABORT_REASON_NEW_INFO                FLOODING_TXABORT_MODE_NEW_INFO
#define FLOODING_TXABORT_REASON_FOREIGN_RESPONSIBILITY  FLOODING_TXABORT_MODE_ACKED       /* Take foreign resp. as acked */
#define FLOODING_TXABORT_REASON_FINISHED                FLOODING_TXABORT_MODE_FINISHED    /* Can be used by other elements e.g. Policies */

  uint32_t _abort_tx_mode;
  uint32_t _tx_aborts;
  uint32_t _tx_aborts_errors;

 private:

  EtherAddress _last_tx_dst_ea;
  EtherAddress _last_tx_src_ea;
  uint16_t _last_tx_bcast_id;
  bool _last_tx_abort;

 public:

  inline void set_last_tx(EtherAddress dst, EtherAddress src, uint16_t id) {
    _last_tx_dst_ea = dst;
    _last_tx_src_ea = src;
    _last_tx_bcast_id = id;
  };

  void reset_last_tx() { set_last_tx(EtherAddress(), EtherAddress(), 0); _last_tx_abort = false; }

  inline bool is_last_tx_id(EtherAddress &src, uint16_t id) {
    return (_last_tx_src_ea == src) && (_last_tx_bcast_id == id);
  }

  bool is_last_tx(EtherAddress &dst, EtherAddress &src, uint16_t id) {
    return ((_last_tx_dst_ea == dst) && is_last_tx_id(src, id));
  }

  void abort_last_tx(EtherAddress &dst, uint32_t abort_reason) {
    if (((_abort_tx_mode & abort_reason) == FLOODING_TXABORT_MODE_NONE) || dst.is_broadcast()) return;

    BRN_DEBUG("Abort: %s %s %d (%s)", _last_tx_dst_ea.unparse().c_str(),
                                      _last_tx_src_ea.unparse().c_str(),
                                      _last_tx_bcast_id, dst.unparse().c_str());

    if (_last_tx_abort ) {
      BRN_WARN("Abort already running");
      return;
    }

    bool failure = false;
    for (int devs = 0; devs < _me->countDevices(); devs++)
      failure |= (_me->getDeviceByIndex(devs)->abort_transmission(dst) != 0);

    if ( failure ) {
      _tx_aborts_errors++;
    } else {
      BRN_DEBUG("Abort last TX");
      _tx_aborts++;
      _last_tx_abort = true;
    }
  }

  inline void abort_last_tx(uint32_t abort_reason) {
    abort_last_tx(_last_tx_dst_ea, abort_reason);
  }

  inline EtherAddress *get_last_tx(uint16_t *id) {
    *id = _last_tx_bcast_id;
    return &_last_tx_src_ea;
  }

  void add_rx_probability(EtherAddress &fwd, EtherAddress &src, uint16_t id, uint32_t no_transmissions);

  Vector<FloodingPolicy *> _schemes;
  FloodingPolicy **_scheme_array;
  uint32_t _max_scheme_id;
  String _scheme_string;
  uint32_t _flooding_strategy;

  int parse_schemes(String s_schemes, ErrorHandler* errh);
  FloodingPolicy *get_flooding_scheme(uint32_t flooding_strategy);

};

CLICK_ENDDECLS
#endif
