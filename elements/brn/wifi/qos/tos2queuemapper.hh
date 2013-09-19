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

#ifndef TOS2QUEUEMAPPERELEMENT_HH
#define TOS2QUEUEMAPPERELEMENT_HH
#include <click/element.hh>

#include <elements/brn/brnelement.hh>
#include <elements/brn/wifi/channelstats.hh>
#include <elements/brn/wifi/collisioninfo.hh>

#include "bo_schemes/backoff_scheme.hh"


CLICK_DECLS

/*
=c
()

=d

*/

#define BACKOFF_STRATEGY_OFF                             0 /* default */
#define BACKOFF_STRATEGY_DIRECT                          1
#define BACKOFF_STRATEGY_MAX_THROUGHPUT                  2
#define BACKOFF_STRATEGY_CHANNEL_LOAD_AWARE              3
#define BACKOFF_STRATEGY_TARGET_PACKETLOSS               4
#define BACKOFF_STRATEGY_LEARNING                        5
#define BACKOFF_STRATEGY_LEARNING_STRICT                 6
#define BACKOFF_STRATEGY_TARGET_DIFF_RXTX_BUSY           7
#define BACKOFF_STRATEGY_EXPONENTIAL_LINEAR              8 /* PLEB */

#define BACKOFF_STRATEGY_REFACTOR                        99

#define TOS2QM_DEFAULT_LEARNING_BO                       63
#define TOS2QM_DEFAULT_TARGET_PACKET_LOSS                10
#define TOS2QM_DEFAULT_TARGET_CHANNELLOAD                90
#define TOS2QM_DEFAULT_TARGET_DIFF_RXTX_BUSY             5
#define TOS2QM_DEFAULT_EXPONENTIAL_LINEAR                31

#define TOS2QM_LEARNING_MIN_CWMIN                        31
#define TOS2QM_LEARNING_MAX_CWMIN                        255

#define TOS2QM_PLEB_MIN_CWMIN                            7
#define TOS2QM_PLEB_MAX_CWMIN                            127

class Tos2QueueMapper : public BRNElement {

public:
  Tos2QueueMapper();
  ~Tos2QueueMapper();

  const char *class_name() const  { return "Tos2QueueMapper"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return "a"; }

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  //    Packet * smaction(Packet *p, int port);
  //    void push(int, Packet *);
  //    Packet *pull(int);
  Packet *simple_action(Packet *p);

  String stats();

  void handle_feedback(Packet *);

  uint16_t _bqs_strategy;//Backoff-Queue Selection Strategy(see above define declarations)
  void set_backoff_strategy(uint16_t value) { _bqs_strategy = value; }
  uint16_t get_backoff_strategy() { return _bqs_strategy; }

  inline uint8_t get_no_queues() { return no_queues; }
  uint32_t get_queue_usage(uint8_t position);

  int find_queue(uint16_t cwmin);
  int find_queue_prob(uint16_t backoff_window_size);
  uint32_t find_closest_backoff(uint32_t bo);
  uint32_t find_closest_backoff_exp(uint32_t bo);

  void reset_queue_usage() { memset(_queue_usage, 0, sizeof(uint32_t) * no_queues); }

private:
  uint32_t set_backoff();
  uint32_t get_backoff();
  uint32_t recalc_backoff_queues(uint32_t backoff, uint32_t tos, uint32_t step);
  void parse_queues(String s_cwmin, String s_cwmax, String s_aifs);
  int parse_bo_schemes(String s_schemes, ErrorHandler* errh);
  void init_stats();
  BackoffScheme *get_bo_scheme(uint16_t strategy);

  ChannelStats *_cst;         //Channel-Statistics-Element (see: ../channelstats.hh)
  CollisionInfo *_colinf;     //Collision-Information-Element (see: ../collisioninfo.hh)

  uint32_t _learning_current_bo;
  uint32_t _learning_count_up;
  uint32_t _learning_count_down;
  uint32_t _learning_max_bo;

private:
  uint8_t no_queues;          //number of queues
  uint16_t *_cwmin;           //Contention Window Minimum; Array (see: monitor)
  uint16_t *_cwmax;           //Contention Window Maximum; Array (see:monitor)
  uint16_t *_aifs;            //Arbitration Inter Frame Space;Array (see 802.11e Wireless Lan for QoS)
  uint32_t *_queue_usage;     //frequency of the used queues

  uint16_t *_bo_exp;           //exponent for backoff in queue
  uint32_t *_bo_usage_usage;   //frequency of the used backoff
  uint32_t _bo_usage_max_no;   //max bo

  uint32_t _ac_stats_id;

  uint32_t _target_packetloss;
  uint32_t _target_channelload;
  uint32_t _bo_for_target_channelload;

public:
  uint32_t _target_diff_rxtx_busy;

  uint32_t _feedback_cnt;
  uint32_t _tx_cnt;
  int32_t _pkt_in_q;

  uint32_t _call_set_backoff;


  BackoffScheme *_current_scheme;
  BackoffScheme **_bo_schemes;

  uint32_t _no_schemes;
  uint16_t _scheme_id;

};

CLICK_ENDDECLS
#endif
