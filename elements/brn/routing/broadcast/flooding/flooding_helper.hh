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
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"


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
  Brn2LinkTable *_link_table;

 public:
  int _max_metric_to_neighbor; // max. metric towards a neighbor

  void print_vector(Vector<EtherAddress> &eas);
  void print_vector(NeighbourMetricList &nodes);
  uint32_t metric2pdr(uint32_t metric);

  // helper
  void get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out);

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

};

CLICK_ENDDECLS
#endif