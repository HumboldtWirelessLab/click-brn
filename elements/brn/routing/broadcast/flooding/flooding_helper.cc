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
    click_chatter("Addr %d : %s", n_i, eas[n_i].unparse().c_str());
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
			if (metric_nb_node > _max_metric_to_neighbor) {
				continue;
			}
			out.push_back(neighbors_tmp[n_i]);
		}
	}
}

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

void
FloodingHelper::filter_known_one_hop_neighbors(const EtherAddress &node, Vector<EtherAddress> &neighbors, Vector<EtherAddress> &filtered_neighbors)
{
  BRN_ERROR("Known Neighborsize before filter: %d", neighbors.size());
  
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

// add all elements from newS to all
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

void
FloodingHelper::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingHelper)
