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
#include "elements/brn/routing/identity/brn2_device.hh"

#include "bo_schemes/backoff_scheme.hh"

CLICK_DECLS

/*
=c
()

=d

*/


#define TOS2QM_ALL_BOS_STATS                             1
#define TOS2QM_BOBUF_SIZE                                4096

#define QUEUEMAPPING_NEXT_BIGGER   0
#define QUEUEMAPPING_PROBABILISTIC 1
#define QUEUEMAPPING_GRAVITATION   2


class Tos2QueueMapper : public BRNElement {

public:
  Tos2QueueMapper();
  ~Tos2QueueMapper();

  const char *class_name() const  { return "Tos2QueueMapper"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return "a"; }

  int initialize(ErrorHandler *);
  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  //    Packet * smaction(Packet *p, int port);
  //    void push(int, Packet *);
  //    Packet *pull(int);
  Packet *simple_action(Packet *p);

  String stats();
  String bos();

  BRN2Device *_device;

  uint32_t _bqs_strategy;                             //Backoff-Queue Selection Strategy(see above define declarations)
  BackoffScheme *get_bo_scheme(uint32_t strategy);
  SchemeList _scheme_list;
  BackoffScheme *_current_scheme;
  void set_backoff_strategy(uint32_t strategy);

  void handle_feedback(Packet *);

  inline uint8_t get_no_queues() { return no_queues; }
  uint32_t get_queue_usage(uint8_t position);

  int find_queue(uint16_t cwmin);
  int find_queue_next_bigger(uint16_t backoff_window_size);
  int find_queue_prob(uint16_t backoff_window_size);
  uint32_t find_closest_backoff(uint32_t bo);
  uint32_t find_closest_backoff_exp(uint32_t bo);

  void reset_queue_usage() { memset(_queue_usage, 0, sizeof(uint32_t) * no_queues); }

private:
  uint32_t set_backoff();
  uint32_t get_backoff();
  uint32_t recalc_backoff_queues(uint32_t backoff, uint32_t tos, uint32_t step);

  void init_stats();

private:

  uint8_t no_queues;          //number of queues
  uint32_t *_cwmin;           //Contention Window Minimum; Array (see: monitor)
  uint32_t *_cwmax;           //Contention Window Maximum; Array (see:monitor)
  uint32_t *_aifs;            //Arbitration Inter Frame Space;Array (see 802.11e Wireless Lan for QoS)

  uint32_t *_queue_usage;     //frequency of the used queues

  uint16_t *_bo_exp;           //exponent for backoff in queue
  uint32_t *_bo_usage_usage;   //frequency of the used backoff
  uint32_t _bo_usage_max_no;   //max bo
  Timestamp *_last_bo_usage;
  int16_t *_all_bos;
  uint16_t _all_bos_idx;

  uint32_t _ac_stats_id;

public:

  uint32_t _feedback_cnt;
  uint32_t _tx_cnt;
  int32_t _pkt_in_q;

  uint32_t _call_set_backoff;

  uint32_t _queue_mapping;
};

CLICK_ENDDECLS
#endif
