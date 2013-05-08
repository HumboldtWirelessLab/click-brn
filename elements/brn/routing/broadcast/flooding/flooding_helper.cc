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

extern "C" {
  static int link_metric_sorter(const void *va, const void *vb, void */*thunk*/) {
      NeighbourMetric *a = (NeighbourMetric*) va, *b = (NeighbourMetric*)vb;

      if ( a->_metric < b->_metric ) return -1;
      if ( a->_metric > b->_metric ) return 1;
      return 0;
  }
}

#if 0
extern "C" {
  static int link_pdr_sorter(const void *va, const void *vb, void */*thunk*/) {
      NeighbourMetric *a = (NeighbourMetric*)va, *b = (NeighbourMetric *)vb;

      if ( a->_metric > b->_metric ) return -1;
      if ( a->_metric < b->_metric ) return 1;
      return 0;
  }
}
#endif

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
  for( int n_i = 0; n_i < nodes.size(); n_i++) { // iterate over all my neighbors
    BRN_DEBUG("Addr %d : %s (%d, %d)", n_i, nodes[n_i]._ea.unparse().c_str(), nodes[n_i]._metric, nodes[n_i]._flags);
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
	if (_link_table) {
		Vector<EtherAddress> neighbors_tmp;

   		_link_table->get_neighbors(node, neighbors_tmp);
	
      		for( int n_i = 0; n_i < neighbors_tmp.size(); n_i++) {
		  	// calc metric between this neighbor and node to make sure that we are well-connected
			int metric_nb_node = _link_table->get_link_metric(node, neighbors_tmp[n_i]);

			// skip to bad neighbors
			if (metric_nb_node > _max_metric_to_neighbor) continue;
			out.push_back(neighbors_tmp[n_i]);
		}
	}
}

/*
 * Check all one hop neighbours and get shortest path to every node of them
 * remove all neighbour nodes which have a better two hop route than single link
 * IMPORTANT: only use one hop neighbours 
 */

void
FloodingHelper::filter_bad_one_hop_neighbors(const EtherAddress &node, Vector<EtherAddress> &neighbors, Vector<EtherAddress> &filtered_neighbors)
{
  BRN_ERROR("Neighborsize before filter: %d", neighbors.size());
  
  NeighbourMetricList neighboursMetric;
  
  if (_link_table) {

    for( int n_i = neighbors.size()-1; n_i >= 0; n_i--) {
      int metric_nb_node = metric2pdr(_link_table->get_link_metric(node, neighbors[n_i]));
      
      neighboursMetric.push_back(NeighbourMetric(neighbors[n_i],metric_nb_node));
    }
    
    click_qsort(neighboursMetric.begin(), neighboursMetric.size(), sizeof(NeighbourMetric), link_metric_sorter);

    /*for( int n_i = 0; n_i < neighboursMetric.size(); n_i++) {
        BRN_ERROR("%d.Neighbour: %d",n_i,neighboursMetric[n_i]._metric);
    }*/
   
    for( int n_i = neighboursMetric.size()-1; n_i >= 0; n_i--) {
      if ( neighboursMetric[n_i]._flags == 0 ) {                               //not fixed
        int metric_nb_node = neighboursMetric[n_i]._metric;
     
	int best_metric = metric_nb_node;
	int best_neighbour = 0;
	
        for( int n_j = 0; n_j < neighboursMetric.size(); n_j++) {
 	  if ( n_j == n_i ) continue;

          int metric_nb_nb = (neighboursMetric[n_j]._metric *
                             metric2pdr(_link_table->get_link_metric(neighboursMetric[n_j]._ea, neighboursMetric[n_i]._ea))) / 100;
         //BRN_ERROR("%d.Neighbour compare (%d) %d <-> %d",n_i,n_j,metric_nb_node, metric_nb_nb );
  
      
        //TODO: use PER/PDR
          if ( metric_nb_nb >= best_metric ) {
	    best_neighbour = n_j;
	    best_metric = metric_nb_nb;
	  }
        }

        if ( best_metric >= metric_nb_node ) {
            neighboursMetric[best_neighbour]._flags = 1;           //fix
            neighboursMetric.erase(neighboursMetric.begin() + n_i);
	} 
      }
    }
  }
  
  for( int n_i = 0; n_i < neighboursMetric.size(); n_i++) {
    //BRN_ERROR("%d.Neighbour: %d %d",n_i,neighboursMetric[n_i]._metric,neighboursMetric[n_i]._flags);
    filtered_neighbors.push_back(neighboursMetric[n_i]._ea);
  }
  
  BRN_ERROR("Neighborsize after filter: %d", filtered_neighbors.size());
}


/*
 * Dijkstra helper
 */
void
FloodingHelper::get_graph(const EtherAddress &start_node, NeighbourMetricList &neighboursMetric, NeighbourMetricMap &neighboursMetricMap, uint32_t hop_count)
{
  Vector<EtherAddress> nn_list;                                              //neighbourlist

  neighboursMetric.clear();
  neighboursMetricMap.clear();
  nn_list.clear();
  
  neighboursMetric.push_back(NeighbourMetric(start_node, 0, 0));
  neighboursMetricMap.insert(start_node,&(neighboursMetric[neighboursMetric.size()-1]));
  
  uint32_t new_nodes_start = 0;
  uint32_t new_nodes_end = 1;
  
  for ( uint32_t h = 1; h <= hop_count; h++ ) {    
    for ( uint32_t n = new_nodes_start; n < new_nodes_end; n++ ) {
      get_filtered_neighbors(neighboursMetric[n]._ea, nn_list);                   // get n of n

      for( int nn_i = nn_list.size()-1; nn_i >= 0; nn_i--) {                      //check all n of n 
	if ( neighboursMetricMap.findp(nn_list[nn_i]) == NULL ) {                 // if not in list
          neighboursMetric.push_back(NeighbourMetric(nn_list[nn_i], 0, h));       // add 
          neighboursMetricMap.insert(nn_list[nn_i],&(neighboursMetric[neighboursMetric.size()-1]));
	}
      }
    }
    new_nodes_start = new_nodes_end;
    new_nodes_end = neighboursMetric.size();
    nn_list.clear();
  }
}



/*
 * Check all one hop neighbours and get shortest path to every node of them
 * remove all neighbour nodes which have a better two hop route than single link
 * 
 * Local Dijkstra
 */

void
FloodingHelper::filter_bad_one_hop_neighbors_with_all_known_nodes(const EtherAddress &node, Vector<EtherAddress> &known_neighbors, Vector<EtherAddress> &/*filtered_neighbors*/)
{
  uint32_t no_nodes;
  
  NeighbourMetricList neighboursMetric;
  NeighbourMetricMap neighboursMetricMap;
  
  bool metric_changed;
  
  if (!_link_table) return;
  
  //get all n-hop nodes
  get_graph(node, neighboursMetric, neighboursMetricMap, 2 ); //2 hops
  
  no_nodes = neighboursMetric.size();
  
  // metric_map[a * no_nodes + b] = metric2pdr(_link_table->get_link_metric(neighboursMetric[a]._ea, neighboursMetric[b]._ea));
  
  //set metric to known nodes to 100
  neighboursMetric[0]._predecessor = &neighboursMetric[0];
  
  for ( int i = 0; i < known_neighbors.size(); i++ ) {
    NeighbourMetric *nm = neighboursMetricMap.find(known_neighbors[i]);
    nm->_metric = 100;
    nm->_hops = 0;
    nm->_predecessor = &neighboursMetric[0];
  }
      
  do {
    metric_changed = false;
    
    
    
    for ( uint32_t marked = 0; marked < no_nodes; marked++);
    
  } while ( metric_changed );
    
  //delete metric_map;
}
/*
 * remove nose of set "neighbors" which are in "known_neighbors". Result: filtered_neighbors
 * 
 */
void
FloodingHelper::filter_known_one_hop_neighbors(Vector<EtherAddress> &neighbors, Vector<EtherAddress> &known_neighbors, Vector<EtherAddress> &filtered_neighbors)
{
  HashMap<EtherAddress, EtherAddress> node_hm;
  
  for( int i = 0; i < known_neighbors.size(); i++) node_hm.insert(known_neighbors[i],known_neighbors[i]);  //known nodes to hashmap (faster access)

  for( int i = 0; i < neighbors.size(); i++)                                                               //check each neighbour wheter he is known or not
    if ( node_hm.findp(neighbors[i]) == NULL )
      filtered_neighbors.push_back(neighbors[i]);                                                          //add only unknown
    
  node_hm.clear();
}

/*
 * count node which are member of set 1 but not in set 2
 * 
 */
int
FloodingHelper::subtract_and_cnt(const Vector<EtherAddress> &s1, const Vector<EtherAddress> &s2)
{
	int diff_cnt = 0;
      	
	for( int s1_i = 0; s1_i < s1.size(); s1_i++) { // loop over s1
		bool found = false;
		for( int s2_i = 0; s2_i < s2.size(); s2_i++) { // loop over s2
			if (s1[s1_i] == s2[s2_i]) {
				found = true;
				break;
			}
		}
		if (!found) {
			diff_cnt = diff_cnt + 1;
		}
	}
	return diff_cnt;
}

/*
 *  add all from elements from newS to all
 *
 */ 
void
FloodingHelper::addAll(const Vector<EtherAddress> &newS, Vector<EtherAddress> &all)
{
	for( int s1_i = 0; s1_i < newS.size(); s1_i++) { // loop over newS
		bool found = false;
		for( int s2_i = 0; s2_i < all.size(); s2_i++) { // loop over all
			if (newS[s1_i] == all[s2_i]) {
				found = true;
				break;
			}
		}
		if (!found) {
			all.push_back(newS[s1_i]);
		}
	}
}

/*
 *  find node in the set with worst link
 *
 */ 
int
FloodingHelper::findWorst(const EtherAddress &src, Vector<EtherAddress> &neighbors)
{
  if ( (!_link_table) || (neighbors.size() == 0) ) return -1;
  
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
