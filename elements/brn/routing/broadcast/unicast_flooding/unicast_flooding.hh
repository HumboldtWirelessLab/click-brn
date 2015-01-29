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

#ifndef UNICAST_FLOODING_ELEMENT_HH
#define UNICAST_FLOODING_ELEMENT_HH

#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/routing/broadcast/flooding/flooding.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_helper.hh"


CLICK_DECLS
/**
 * TODO: check wehther assined nodes can be real acked_nodes (should never happend)
 */

/*
 * =c
 * UnicastFlooding()
 * =s converts a broadcast packet into a unicast packet
 * output: rewrited packet
 * =d
 *
 * Restrictions: works only together with ETX metric
 */

#define UNICAST_FLOODING_PRESELECTION_ALL_NODES         0
#define UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED  1
#define UNICAST_FLOODING_PRESELECTION_CHILD_ONLY        2

#define UNICAST_FLOODING_NO_REWRITE              0
#define UNICAST_FLOODING_BCAST_WITH_PRECHECK     1
#define UNICAST_FLOODING_STATIC_REWRITE          2
#define UNICAST_FLOODING_ALL_UNICAST             3
#define UNICAST_FLOODING_TAKE_BEST               4
#define UNICAST_FLOODING_TAKE_WORST              5
#define UNICAST_FLOODING_MOST_NEIGHBOURS         6
#define UNICAST_FLOODING_PRIO_LOW_BENEFIT        7
#define UNICAST_FLOODING_RANDOM                  8

#define UNICAST_FLOODING_STATS_TARGET_SIZE       8

#define UNICAST_FLOODING_DIJKSTRA_PDR          112

class UnicastFlooding : public BRNElement {
 public:

  typedef HashMap<EtherAddress, uint32_t> TargetRewriteCntMap;
  typedef TargetRewriteCntMap::const_iterator TargetRewriteCntMapIter;

  //
  //methods
  //
  UnicastFlooding();
  ~UnicastFlooding();

  const char *class_name() const  { return "UnicastFlooding"; }
  /**
   * 0: output
   * 1: reject
   **/
  const char *port_count() const  { return "1/2"; }
  const char *processing() const  { return "a/ah"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  void push(int, Packet *);
  Packet *pull(int);
  Packet *smaction(Packet *p_in, bool is_push);

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

 public: 
  //
  //member
  //
  BRN2NodeIdentity *_me;
  Flooding *_flooding;
  FloodingHelper *_fhelper;
  FloodingDB *_flooding_db;

 private:
  int _max_metric_to_neighbor;         // max. metric towards a neighbor
  int _cand_selection_strategy;        // the way we choose the candidate for unicast forwarding
  uint32_t _pre_selection_mode;
  uint32_t _ucast_peer_metric;
  uint32_t _ucast_per_node_limit;
  bool _reject_on_empty_cs;
  EtherAddress static_dst_mac;

  EtherAddress algorithm_most_neighbours(Vector<EtherAddress> &neighbors, int hops);
  EtherAddress algorithm_less_neighbours(Vector<EtherAddress> &neighbors, int hops);
  EtherAddress algorithm_neighbours(Vector<EtherAddress> &neighbors, int hops, bool most);

 public:
  bool _force_responsibility;
  bool _use_assign_info;
  bool _fix_candidate_set;

  uint32_t _self_pdr, _self_pdr_steps, _self_pdr_limit;

  void add_rewrite(EtherAddress *src, uint16_t id, EtherAddress *target);

  void set_strategy(int s) { _cand_selection_strategy = s; }
  int get_strategy() { return _cand_selection_strategy; }
  String get_strategy_string(uint32_t id);

  void set_preselection(uint32_t p) { _pre_selection_mode = p; }
  int get_preselection() { return _pre_selection_mode; }
  String get_preselection_string(uint32_t id);
  int get_ucast_peer_metric() { return _ucast_peer_metric; }
  String get_reject_on_empty_cs() { return String(_reject_on_empty_cs); }

  void set_static_mac(EtherAddress *mac) { static_dst_mac = *mac; }
  EtherAddress *get_static_mac() { return &static_dst_mac; }

  EtherAddress last_unicast_used;
  uint32_t _cnt_rewrites;
  uint32_t _cnt_bcasts;
  uint32_t _cnt_bcasts_empty_cs;
  uint32_t _cnt_reject_on_empty_cs;

  TargetRewriteCntMap rewrite_cnt_map;

  Vector<Packet*> all_unicast_pkt_queue;

  EtherAddress _last_tx_dst_ea;
  EtherAddress _last_tx_src_ea;
  uint16_t _last_tx_bcast_id;

};

CLICK_ENDDECLS
#endif
