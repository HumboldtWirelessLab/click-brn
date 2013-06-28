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

/*
 * flooding_flooding.{cc,hh} -- converts a broadcast packet into a unicast packet
 * R.Sombrutzki
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brn2.h"

#include "flooding_helper.hh"


CLICK_DECLS

//TODO: refactor: move to brntools
void 
FloodingHelper::print_vector(Vector<EtherAddress> &eas)
{
  for( int n_i = 0; n_i < eas.size(); n_i++) { // iterate over all my neighbors
    BRN_DEBUG("Addr %d : %s", n_i, eas[n_i].unparse().c_str());
  }
}

void 
FloodingHelper::print_vector(NeighbourMetricList &nodes)
{
  for( int node = 0; node < nodes.size(); node++) { // iterate over all my neighbors
   if ( nodes[node]->_predecessor != NULL ) {
      BRN_DEBUG("Node: %s Pre: %s Metric: %d Hops: %d Flags: %d", nodes[node]->_ea.unparse().c_str(),
	  	                                   nodes[node]->_predecessor->_ea.unparse().c_str(),
		                                   nodes[node]->_metric, nodes[node]->_hops,
		                                   nodes[node]->_flags);
    } else {
      BRN_DEBUG("Node: %s Pre: NULL Metric: %d Hops: %d Flags: %d", nodes[node]->_ea.unparse().c_str(),
		                                   nodes[node]->_metric, nodes[node]->_hops,
		                                   nodes[node]->_flags);
    }
  }
}

//TODO: more generic (not only ETX, use Linkstats instead or ask metric-class
uint32_t
FloodingHelper::metric2pdr(uint32_t metric)
{
  if ( metric == 0 ) return 100;
  if ( metric == BRN_LT_INVALID_LINK_METRIC ) return 0;
  
  return (1000 / isqrt32(metric));
}

FloodingHelper::FloodingHelper():
  _link_table(NULL)
{
  BRNElement::init();
}

FloodingHelper::~FloodingHelper()
{
}

int
FloodingHelper::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      "MAXNBMETRIC", cpkP+cpkM, cpInteger, &_max_metric_to_neighbor,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
FloodingHelper::initialize(ErrorHandler *)
{
  return 0;
}

void
FloodingHelper::uninitialize()
{
  //cleanup
}

//-----------------------------------------------------------------------------
// Helper methods
//-----------------------------------------------------------------------------

/*
 * Neighbours with linkmetrik higher than given threshold
 * 
 */
void
FloodingHelper::get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out)
{
  if ( out.size() != 0 ) {
    BRN_ERROR("get_filtered_neighbors: out size not 0. Call clear!");
    out.clear();
  }

  Vector<EtherAddress> neighbors_tmp;

  _link_table->get_neighbors(node, neighbors_tmp);

  for( int n_i = 0; n_i < neighbors_tmp.size(); n_i++) {
    // calc metric between this neighbor and node to make sure that we are well-connected
    //BRN_DEBUG("Check Neighbour: %s",neighbors_tmp[n_i].unparse().c_str());
    int metric_nb_node = _link_table->get_link_metric(node, neighbors_tmp[n_i]);

    // skip to bad neighbors
    if (metric_nb_node > _max_metric_to_neighbor) {
      BRN_DEBUG("Skip bad neighbor %s (%d)", neighbors_tmp[n_i].unparse().c_str(),metric_nb_node);
      continue;
    }

    out.push_back(neighbors_tmp[n_i]);
  }
  //BRN_DEBUG("filter finished: %d",out.size());	
}

void
FloodingHelper::init_graph(const EtherAddress &start_node, NetworkGraph &ng, int src_metric)
{
  clear_graph(ng);
  
  ng.nml.push_back(new NeighbourMetric(start_node, src_metric, 0));
  ng.nmm.insert(start_node,ng.nml[ng.nml.size()-1]);
}


/*
 * Dijkstra helper
 */
void
FloodingHelper::get_graph(NetworkGraph &ng, uint32_t hop_count, int /*src_metric*/)
{
  Vector<EtherAddress> nn_list;                                              //neighbourlist
 
  uint32_t new_nodes_start = 0;
  uint32_t new_nodes_end = 1;
  
  for ( uint32_t h = 1; h <= hop_count; h++ ) {    
    for ( uint32_t n = new_nodes_start; n < new_nodes_end; n++ ) {
      get_filtered_neighbors(ng.nml[n]->_ea, nn_list);                   // get n of n

      for( int nn_i = nn_list.size()-1; nn_i >= 0; nn_i--) {             //check all n of n 
	if ( ng.nmm.findp(nn_list[nn_i]) == NULL ) {                     // if not in list
          ng.nml.push_back(new NeighbourMetric(nn_list[nn_i], 0, h));    // add 
          ng.nmm.insert(nn_list[nn_i],ng.nml[ng.nml.size()-1]);
	}
      }
      nn_list.clear();
    }
    new_nodes_start = new_nodes_end;
    new_nodes_end = ng.nml.size();
  }
}

/*
 * get graph with blacklist
 */
void
FloodingHelper::get_graph(NetworkGraph &ng, uint32_t hop_count, int /*src_metric*/, HashMap<EtherAddress,EtherAddress> &blacklist)
{
  Vector<EtherAddress> nn_list;                                              //neighbourlist
  
  uint32_t new_nodes_start = 0;
  uint32_t new_nodes_end = 1;
  
  for ( uint32_t h = 1; h <= hop_count; h++ ) {    
    for ( uint32_t n = new_nodes_start; n < new_nodes_end; n++ ) {
      get_filtered_neighbors(ng.nml[n]->_ea, nn_list);                   // get n of n

      for( int nn_i = nn_list.size()-1; nn_i >= 0; nn_i--) {             //check all n of n 
	if ((ng.nmm.findp(nn_list[nn_i]) == NULL) && 
	    (blacklist.findp(nn_list[nn_i]) == NULL)) {                  // if not in list and not in blacklist
          ng.nml.push_back(new NeighbourMetric(nn_list[nn_i], 0, h));    // add 
          ng.nmm.insert(nn_list[nn_i],ng.nml[ng.nml.size()-1]);
	}
      }
      nn_list.clear();
    }
    new_nodes_start = new_nodes_end;
    new_nodes_end = ng.nml.size();
  }
}

/*
 * clear graph
 */
void
FloodingHelper::clear_graph(NetworkGraph &ng)
{
  for ( int i = 0; i < ng.nml.size(); i++ )
    delete ng.nml[i];
  
  ng.nml.clear();
  ng.nmm.clear();
}

/*
 * remove nodes
 */
void
FloodingHelper::remove_node(EtherAddress &node, NetworkGraph &ng)
{
  ng.remove_node(node);
}

/*
 * Check all one hop neighbours and get shortest path to every node of them
 * remove all neighbour nodes which have a better two hop route than single link
 * 
 * Local Dijkstra
 */
void
FloodingHelper::get_local_graph(const EtherAddress &node, Vector<EtherAddress> &known_neighbors, NetworkGraph &ng, int hops, int src_metric)
{
  BRN_DEBUG("filter_bad_one_hop_neighbors_with_all_known_nodes");
  uint32_t no_nodes;
  
  bool metric_changed;
  
  if (!_link_table) return;
    
  //get all n-hop nodes with minimum metric
  init_graph(node, ng, src_metric);
  get_graph(ng, hops, src_metric);

  //add known nodes if they are not included
  for ( int i = 0; i < known_neighbors.size(); i++ ) {
    if ( ng.nmm.findp(known_neighbors[i]) == NULL) {
      ng.nml.push_back(new NeighbourMetric(known_neighbors[i], 0, 0));       // add 
      ng.nmm.insert(known_neighbors[i],ng.nml[ng.nml.size()-1]);
    }
  }
  
  //store size of neighbours
  no_nodes = ng.nml.size();
 
  BRN_DEBUG("NeighbourSize: %d KnownNeighbours: %d",no_nodes,known_neighbors.size());
  
  //set metric to known nodes to 100
  ng.nml[0]->_predecessor = ng.nml[0];
  ng.nml[0]->set_root_follower_flag();

  for ( int i = 0; i < known_neighbors.size(); i++ ) {
    if ( ng.nmm.findp(known_neighbors[i]) == NULL ) {
      BRN_FATAL("No Metric for neighbour %s",known_neighbors[i].unparse().c_str());
      continue;
    }
    NeighbourMetric *nm = ng.nmm.find(known_neighbors[i]);
    nm->_metric = 100;
    nm->_hops = 0;
    nm->_predecessor = nm;
  }

  int best_metric, best_metric_src, best_metric_dst;
  
  do {
    metric_changed = false;
  
    best_metric = best_metric_src = best_metric_dst = -1;
    int new_best_metric;

    for ( uint32_t src_node = 0; src_node < no_nodes; src_node++) {
      for ( uint32_t dst_node = 1; dst_node < no_nodes; dst_node++) {
	if ((dst_node == src_node) ||
	    (ng.nml[dst_node]->_predecessor != NULL) ||
	    (ng.nml[src_node]->_predecessor == NULL)) continue;
	
	new_best_metric = (ng.nml[src_node]->_metric *
	                   metric2pdr(_link_table->get_link_metric(ng.nml[src_node]->_ea, ng.nml[dst_node]->_ea))) / 100;
  
        if (new_best_metric >= best_metric) {
	  best_metric = new_best_metric;
	  best_metric_dst = dst_node;
	  best_metric_src = src_node;
	}
      }
    }
    
    if ( best_metric_dst != best_metric_src ) {
      metric_changed = true;
      ng.nml[best_metric_dst]->_metric = best_metric;
      ng.nml[best_metric_dst]->_predecessor = ng.nml[best_metric_src];
      ng.nml[best_metric_dst]->_hops = ng.nml[best_metric_dst]->_predecessor->_hops + 1;
      ng.nml[best_metric_dst]->copy_root_follower_flag(ng.nml[best_metric_dst]->_predecessor);
    }

  } while ( metric_changed );

  print_vector(ng.nml);
  
  BRN_DEBUG("filter_bad_one_hop_neighbors_with_all_known_nodes: end");

}

void
FloodingHelper::get_local_childs(const EtherAddress &node, NetworkGraph &ng, int hops)
{
  BRN_DEBUG("get local_childs");
      
  if (!_link_table) return;
  
  //start node is blacklist node
  HashMap<EtherAddress, EtherAddress> blacklist;
  blacklist.insert(node, node);
  
  //get neighbours
  Vector<EtherAddress> neighbours;
  get_filtered_neighbors(node, neighbours);
  
  NetworkGraph *ng_list = new NetworkGraph[neighbours.size()];
  int *ng_ids = new int[neighbours.size()];
    
  //get neighbours of neighbours  without start node
  for ( int i = 0; i < neighbours.size(); i++ ) {
    init_graph(neighbours[i], ng_list[i], 100);
    get_graph(ng_list[i], hops, 100, blacklist);
    ng_ids[i] = i;
  }
  
  //merge graphs
  int s,b;
  for ( int g1 = 0; g1 < neighbours.size()-1; g1++ ) {
    for ( int g2 = g1+1; g2 < neighbours.size(); g2++ ) {
      if ( ng_ids[g1] == ng_ids[g2] ) continue;
      if ( graph_overlapping(ng_list[g1],ng_list[g2]) ) {
        if ( ng_ids[g1] < ng_ids[g2] ) {
          s = ng_ids[g1];
          b = ng_ids[g2];
        } else {
          s = ng_ids[g1];
          b = ng_ids[g2];
        }

        for ( int i = 0; i < neighbours.size(); i++)
          if (ng_ids[i] == b) ng_ids[i] = s;
      }
    }
  }
  
  //search single graphs
  for ( int g1 = 0; g1 < neighbours.size(); g1++ ) {
    if ( ng_ids[g1] >= g1 ) { 
      int g2;
      for ( g2 = g1+1; g2 < neighbours.size(); g2++ ) 
        if ( ng_ids[g1] == g1 ) break;
      
      if ( g2 == neighbours.size() ) {
        //node has non-overlapping graph
        ng.add_node(neighbours[g1], 100, 1);
	ng.nml[ng.size()-1]->set_root_follower_flag(); //set rot_follower: is neighbour and this is necessary for get_candidate_set
      }
    }
     
    clear_graph(ng_list[g1]);
  }
  
  neighbours.clear();
  delete[] ng_list;
  delete[] ng_ids;
}

void
FloodingHelper::get_candidate_set(NetworkGraph &ng, Vector<EtherAddress> &candidate_set)
{
  if ( candidate_set.size() != 0 ) {
    BRN_ERROR("Candidate set is not empty! Clear.");
    candidate_set.clear();
  }
  
  for( int node = 0; node < ng.nml.size(); node++) { 
     if ( ng.nml[node]->is_root_follower() && (ng.nml[node]->_hops == 1) ) {
       candidate_set.push_back(ng.nml[node]->_ea);
     }
  }  
}

/*
 * count node which are member of set 1 but not in set 2
 * 
 */
int
FloodingHelper::graph_diff_size(NetworkGraph &ng, NetworkGraph &ng2)
{
        int diff_cnt = 0;
      	
	for( int s1_i = 0; s1_i < ng.size(); s1_i++) { // loop over s1
	  if ( ng2.nmm.findp(ng.nml[s1_i]->_ea) == NULL ) diff_cnt++;
	}
	return diff_cnt;
}

int
FloodingHelper::graph_diff_size(Vector<EtherAddress> &ng, Vector<EtherAddress> &ng2)
{
	int diff_cnt = 0;
	HashMap<EtherAddress,bool> lookup;
	
	for( int i = 0; i < ng2.size(); i++) lookup.insert(ng2[i], true);

	for( int s1_i = 0; s1_i < ng.size(); s1_i++) { // loop over s1
	  if ( lookup.findp(ng[s1_i]) == NULL ) diff_cnt++;
	}
	
	lookup.clear();

	return diff_cnt;
}

int
FloodingHelper::graph_diff_size(Vector<EtherAddress> &ng, NetworkGraph &ng2)
{
        int diff_cnt = 0;

        for( int s1_i = 0; s1_i < ng.size(); s1_i++) { // loop over s1
          if ( ng2.nmm.findp(ng[s1_i]) == NULL ) diff_cnt++;
        }
        return diff_cnt;
}


int
FloodingHelper::graph_cut_size(Vector<EtherAddress> &ng, NetworkGraph &ng2)
{
  int cut_cnt = 0;

  for( int s1_i = 0; s1_i < ng.size(); s1_i++) { // loop over s1
    if ( ng2.nmm.findp(ng[s1_i]) != NULL ) cut_cnt++;
  }
    
  return cut_cnt;
}

int
FloodingHelper::graph_cut_size(Vector<EtherAddress> &ng, Vector<EtherAddress> &ng2)
{
  int cut_cnt = 0;

  HashMap<EtherAddress,bool> lookup;
  for( int i = 0; i < ng2.size(); i++) lookup.insert(ng2[i], true);

  for( int s1_i = 0; s1_i < ng.size(); s1_i++) { // loop over s1
    if ( lookup.findp(ng[s1_i]) != NULL ) cut_cnt++;
  }
  
  lookup.clear();
   
  return cut_cnt;
  
}

bool
FloodingHelper::graph_overlapping(NetworkGraph &ng, NetworkGraph &ng_2)
{
  if ( ng.size() < ng_2.size() ) {
    for( int i = 0; i < ng.size(); i++)   // loop over s
      if ( ng_2.nmm.findp(ng.nml[i]->_ea) != NULL ) return true;
  } else {
    for( int i = 0; i < ng_2.size(); i++) // loop over s
      if ( ng.nmm.findp(ng_2.nml[i]->_ea) != NULL ) return true;
  }
  return false;  
}

/*
 *  add all from elements from newS to all
 *
 */ 
void
FloodingHelper::graph_add(NetworkGraph &ng, NetworkGraph &ng_add)
{
  for( int s1_i = 0; s1_i < ng_add.nml.size(); s1_i++) { // loop over ng
    if ( ng.nmm.findp(ng_add.nml[s1_i]->_ea) == NULL ) {
      ng.add_node(ng_add.nml[s1_i]->_ea,ng_add.nml[s1_i]->_metric, ng_add.nml[s1_i]->_hops);
    }
  }
}

void
FloodingHelper::graph_diff(NetworkGraph &ng, NetworkGraph &ng2)
{
  for( int i = 0; i < ng2.nml.size(); i++)
    ng.remove_node(ng2.nml[i]->_ea);
}
  

void
FloodingHelper::graph_cut(NetworkGraph &ng, NetworkGraph &ng2)
{
  for( int i = ng.nml.size()-1; i >= 0; i--)
    if ( ng2.nmm.findp(ng.nml[i]->_ea) == NULL )
      ng.remove_node(ng.nml[i]->_ea);  
}

/*
 *  find node in the set with worst link
 *
 */ 
int
FloodingHelper::find_worst(const EtherAddress &src, Vector<EtherAddress> &neighbors)
{
  if (neighbors.size() == 0) return -1;
  
  int w_met = _link_table->get_link_metric(src, neighbors[0]);
  int w_ind = 0;
  
  for( int i = 1; i < neighbors.size(); i++) { // loop over neighbors
    int m = _link_table->get_link_metric(src, neighbors[i]);
    
    if ( m > w_met ) {
      w_met = m;
      w_ind = i;
    }
  }
  
  return w_ind;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
FloodingHelper::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingHelper)
