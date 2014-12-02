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

#ifndef FLOODING_HELPER_ELEMENT_HH
#define FLOODING_HELPER_ELEMENT_HH

#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>

#include "elements/brn/brnelement.hh"
#include "floodinglinktable.hh"

CLICK_DECLS

/*
 * =c
 * FloodingHelper()
 * =s converts a broadcast packet into a unicast packet
 * output: rewrited packet
 * =d
 *
 */

class NeighbourMetric {
 public:
  EtherAddress _ea;
  uint16_t     _metric;
  uint8_t      _flags;
  uint8_t      _hops;
  NeighbourMetric *_predecessor;

#define NEIGHBOUR_METRIC_ROOT_FOLLOWER 1

  NeighbourMetric(EtherAddress ea, uint16_t metric ) {
    init(ea, metric, 0, 0);
  }

  NeighbourMetric(EtherAddress ea, uint16_t metric, uint8_t hops ) {
    init(ea, metric, 0, hops);
  }

  void init(EtherAddress ea, uint16_t metric, uint8_t flags, uint8_t hops) {
    _ea = ea;
    _metric = metric;
    _flags = flags;
    _hops = hops;
    _predecessor = NULL;
  }

  inline void set_root_follower_flag() {
    _flags |= NEIGHBOUR_METRIC_ROOT_FOLLOWER; 
  }

  inline bool is_root_follower() {
    return ( _flags & NEIGHBOUR_METRIC_ROOT_FOLLOWER ) != 0;
  }

  inline void copy_root_follower_flag(NeighbourMetric *src) {
    _flags |= (src->_flags & NEIGHBOUR_METRIC_ROOT_FOLLOWER);
  }
};

typedef Vector<NeighbourMetric*> NeighbourMetricList;
typedef NeighbourMetricList::const_iterator NeighbourMetricListIter;

typedef HashMap<EtherAddress, NeighbourMetric*> NeighbourMetricMap;
typedef NeighbourMetricMap::const_iterator NeighbourMetricMapIter;

class NetworkGraph {
 public:
  NeighbourMetricList nml;
  NeighbourMetricMap nmm;

  void add_node(EtherAddress ea, uint16_t metric, uint8_t hops) {
    nml.push_back(new NeighbourMetric(ea, metric, hops));
    nmm.insert(ea, nml[nml.size()-1]);
  }

  void remove_node(EtherAddress node) {
    if ( nmm.findp(node) == NULL ) return;
    for ( int i = 0; i < nml.size(); i++ ) {
      if ( nml[i]->_ea == node ) {
        delete nml[i];
        nml.erase(nml.begin() + i);
        nmm.remove(node);
        return;
      }
    }
  }

  int size() { return nml.size(); }
};

class CachedNeighborsMetricList {
 public:
  EtherAddress _node;
  uint8_t _min_pdr_to_neighbor; //pdr

  Vector<EtherAddress> _neighbors;
  uint8_t *_metrics;

  Timestamp _last_update;

  CachedNeighborsMetricList(EtherAddress ea, uint8_t metric) {
    _node = ea;
    _neighbors.clear();
    _metrics = NULL;
    _min_pdr_to_neighbor = metric;
  }

  ~CachedNeighborsMetricList() {
    _neighbors.clear();
    if ( _metrics != NULL ) {
      delete[] _metrics;
      _metrics = NULL;
    }
  }

  void update(FloodingLinktable *lt) {
    _neighbors.clear();
    if ( _metrics != NULL ) {
      delete[] _metrics;
      _metrics = NULL;
    }

    lt->get_neighbors(_node, _neighbors);

    int c_neighbors = _neighbors.size();
    _metrics = new uint8_t[c_neighbors];

    for( int n_i = 0; n_i < c_neighbors;) {
      //calc metric between this neighbor and node to make sure that we are well-connected
      //BRN_DEBUG("Check Neighbour: %s",neighbors_tmp[n_i].unparse().c_str());
      uint8_t metric_nb_node = lt->get_link_pdr(_node, _neighbors[n_i]);

      // skip to bad neighbors
      if (metric_nb_node < _min_pdr_to_neighbor) {
        //click_chatter("Skip bad neighbor %s (%d)", _neighbors[n_i].unparse().c_str(),metric_nb_node);
        c_neighbors--;
        _neighbors.erase(_neighbors.begin() + n_i);
      } else {
        _metrics[n_i] = metric_nb_node;
        n_i++;
      }
    }

    _last_update = Timestamp::now();
  }

  inline int age() { return (Timestamp::now() - _last_update).msecval(); }

  inline int size() { return _neighbors.size(); }

  uint8_t get_metric(EtherAddress &ea) {
    for( int i = 0; i < _neighbors.size();i++ )
      if ( _neighbors[i] == ea ) return _metrics[i];
    return -1;
  }
};

typedef HashMap<EtherAddress, CachedNeighborsMetricList*> CachedNeighborsMetricListMap;
typedef CachedNeighborsMetricListMap::const_iterator CachedNeighborsMetricListMapIter;

class FloodingHelper : public BRNElement {

 public:

  //
  //methods
  //
  FloodingHelper();
  ~FloodingHelper();

  const char *class_name() const  { return "FloodingHelper"; }
  const char *port_count() const  { return "0/0"; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

public:
  //
  //member
  //
  FloodingLinktable *_link_table;

  CachedNeighborsMetricListMap _cnmlmap;
  int _max_metric_to_neighbor; // max. metric towards a neighbor
  int _min_pdr_to_neighbor;    // min. pdr towards a neighbor

#define FLOODINGHELPER_DEFAULTTIMEOUT 10000
  int _cache_timeout;

  uint32_t *_pdr_cache;
#define FLOODINGHELPER_PDR_CACHE_SHIFT 4
  uint32_t _pdr_cache_shift;
  uint32_t _pdr_cache_size;

  void print_vector(Vector<EtherAddress> &eas);
  void print_vector(NeighbourMetricList &nodes);

  uint32_t _better_link_min_ratio;

  // helper
  void get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out, int min_metric = 0);
  CachedNeighborsMetricList* get_filtered_neighbors(const EtherAddress &node, int min_metric = 0);

  void init_graph(const EtherAddress &start_node, NetworkGraph &ng, int src_metric);
  void get_graph(NetworkGraph &net_graph, uint32_t hop_count, int src_metric);
  void get_graph(NetworkGraph &net_graph, uint32_t hop_count, int src_metric, HashMap<EtherAddress,EtherAddress> &blacklist);

  void clear_graph(NetworkGraph &net_graph);
  void remove_node(EtherAddress &node, NetworkGraph &ng);

  void get_local_graph(const EtherAddress &node, Vector<EtherAddress> &known_neighbors, NetworkGraph &net_graph, int hops, int src_metric);
  void get_local_childs(const EtherAddress &node, NetworkGraph &net_graph, int hops);

  void get_candidate_set(NetworkGraph &net_graph, Vector<EtherAddress> &candidate_set);

  int graph_diff_size(NetworkGraph &ng, NetworkGraph &ng2);
  int graph_diff_size(Vector<EtherAddress> &ng, Vector<EtherAddress> &ng2);
  int graph_diff_size(Vector<EtherAddress> &ng, NetworkGraph &ng2);

  int graph_cut_size(Vector<EtherAddress> &ng, NetworkGraph &ng2);
  int graph_cut_size(Vector<EtherAddress> &ng, Vector<EtherAddress> &ng2);

  bool graph_overlapping(NetworkGraph &ng, NetworkGraph &ng_2);
  void graph_add(NetworkGraph &ng, NetworkGraph &ng2);
#define graph_union graph_add
  void graph_diff(NetworkGraph &ng, NetworkGraph &ng2);
  void graph_cut(NetworkGraph &ng, NetworkGraph &ng2);

  int find_worst(const EtherAddress &src, Vector<EtherAddress> &neighbors);
  int find_best(const EtherAddress &src, Vector<EtherAddress> &neighbors);

  bool is_better_fwd(const EtherAddress &src, const EtherAddress &src2, const EtherAddress &dst, uint32_t min_ratio = 100);

};

CLICK_ENDDECLS
#endif
