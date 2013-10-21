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
#define UNICAST_FLOODING_STATIC_REWRITE          1
#define UNICAST_FLOODING_ALL_UNICAST             2
#define UNICAST_FLOODING_TAKE_WORST              3
#define UNICAST_FLOODING_MOST_NEIGHBOURS         4
#define UNICAST_FLOODING_BCAST_WITH_PRECHECK     5

#define UNICAST_FLOODING_STATS_TARGET_SIZE 16

class UnicastFlooding : public BRNElement {
 public:

  class UnicastRewrite {
    struct rewrite_target {
      uint8_t  ea[6];
      uint16_t count;
    };

    uint16_t *id_list;
    struct rewrite_target **target_list;
    uint16_t *target_list_size;
    uint16_t *target_list_max_size;
    
    UnicastRewrite() {
      id_list = new uint16_t[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      target_list = new struct rewrite_target*[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      target_list_size = new uint16_t[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      target_list_max_size = new uint16_t[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      

      memset(id_list,0,DEFAULT_MAX_BCAST_ID_QUEUE_SIZE * sizeof(uint16_t));
      memset(target_list,0,DEFAULT_MAX_BCAST_ID_QUEUE_SIZE * sizeof(struct rewrite_target));
      memset(target_list_size,0,DEFAULT_MAX_BCAST_ID_QUEUE_SIZE * sizeof(uint16_t));
      memset(target_list_max_size,0,DEFAULT_MAX_BCAST_ID_QUEUE_SIZE * sizeof(uint16_t));
    }
  };
    
  typedef HashMap<EtherAddress, uint32_t> TargetRewriteCntMap;
  typedef TargetRewriteCntMap::const_iterator TargetRewriteCntMapIter;

  typedef HashMap<EtherAddress, UnicastRewrite*> TargetRewriteMap;
  typedef TargetRewriteMap::const_iterator TargetRewriteMapIter;

 public:
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

 private:
  int _max_metric_to_neighbor;         // max. metric towards a neighbor
  int _cand_selection_strategy;        // the way we choose the candidate for unicast forwarding
  uint32_t _pre_selection_mode;
  uint32_t _ucast_peer_metric;
  bool _reject_on_empty_cs;
  EtherAddress static_dst_mac;
  bool _force_responsibility;
  bool _use_assign_info;

  EtherAddress algorithm_most_neighbours(Vector<EtherAddress> &neighbors, int hops);

 public:

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
