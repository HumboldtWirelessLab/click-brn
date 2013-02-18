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
 * unicast_flooding.{cc,hh} -- converts a broadcast packet into a unicast packet
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brn2.h"

#include "unicast_flooding.hh"

CLICK_DECLS

extern "C" {
  static int link_metric_sorter(const void *va, const void *vb, void */*thunk*/) {
      NeighbourMetric *a = (NeighbourMetric*) va, *b = (NeighbourMetric*)vb;

      if ( a->_metric < b->_metric ) return -1;
      if ( a->_metric > b->_metric ) return 1;
      return 0;
  }
}

extern "C" {
  static int link_pdr_sorter(const void *va, const void *vb, void */*thunk*/) {
      NeighbourMetric *a = (NeighbourMetric*)va, *b = (NeighbourMetric *)vb;

      if ( a->_metric > b->_metric ) return -1;
      if ( a->_metric < b->_metric ) return 1;
      return 0;
  }
}

//TODO: refactor: move to brntools
void static print_vector(Vector<EtherAddress> &eas)
{
  for( int n_i = 0; n_i < eas.size(); n_i++) { // iterate over all my neighbors
    click_chatter("Addr %d : %s", n_i, eas[n_i].unparse().c_str());
  }
}

//TODO: more generic (not only ETX, use Linkstats instead or ask metric-class
uint32_t static metric2pdr(uint32_t metric)
{
  if ( metric == 0 ) return 100;
  if ( metric == BRN_LT_INVALID_LINK_METRIC ) return 0;
  
  return (1000 / isqrt32(metric));
}

UnicastFlooding::UnicastFlooding():
  _me(NULL),
  _flooding(NULL),
  _link_table(NULL),
  _cand_selection_strategy(0),
  no_rewrites(0)
{
  BRNElement::init();
}

UnicastFlooding::~UnicastFlooding()
{
}

int
UnicastFlooding::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "FLOODINGINFO", cpkP+cpkM, cpElement, &_flooding,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      "MAXNBMETRIC", cpkP+cpkM, cpInteger, &_max_metric_to_neighbor,
      "CANDSELECTIONSTRATEGY", cpkP, cpInteger, &_cand_selection_strategy,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");


  return 0;
}

int
UnicastFlooding::initialize(ErrorHandler *)
{
  click_srandom(_me->getMasterAddress()->hashcode());
  
  last_unicast_used = EtherAddress::make_broadcast();

  return 0;
}

void
UnicastFlooding::uninitialize()
{
  //cleanup
}


/* 
 * process an incoming broadcast/unicast packet:
 * in[0] : // received from other brn node; rewrite from unicast to broadcast
 * in[1] : // transmit to other brn nodes; rewrite from broadcast to unicast
 * out: the rewroted or restored packet
 */
void
UnicastFlooding::push(int port, Packet *p_in)
{
  click_ether *ether = (click_ether *)p_in->ether_header();

  if ( port == 0 ) { // transmit to other brn nodes; rewrite from broadcast to unicast
 
    // next hop must be a broadcast
    EtherAddress next_hop(ether->ether_dhost);
    assert(next_hop.is_broadcast());
    
    // search in my 1-hop neighborhood
    Vector<EtherAddress> neighbors;
    Vector<EtherAddress> filtered_neighbors;
    const EtherAddress *me;

    if ( (_cand_selection_strategy != UNICAST_FLOODING_STATIC_REWRITE) &&
         (_cand_selection_strategy != UNICAST_FLOODING_NO_REWRITE) ) {
      
      if (!_link_table) {
        output(0).push(p_in);
        return;
      } else {
        me = _me->getMasterAddress();
        //_link_table->get_local_neighbors(neighbors);
        get_filtered_neighbors(*me, neighbors);
        filter_bad_one_hop_neighbors(*me, neighbors, filtered_neighbors);

        //TODO: handle if more than 1 source exist
        EtherAddress last_hop = EtherAddress::make_broadcast();

        //filter packet src
        struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)&(p_in->data()[sizeof(struct click_ether) + sizeof(struct click_brn)]);
        EtherAddress src = EtherAddress((uint8_t*)&(p_in->data()[sizeof(click_ether) + sizeof(struct click_brn) + sizeof(struct click_brn_bcast) + bcast_header->extra_data_size])); //src follows header

        BRN_DEBUG("Src %s id: %d",src.unparse().c_str(), ntohs(bcast_header->bcast_id));
    
        _flooding->get_last_node(&src, ntohs(bcast_header->bcast_id), &last_hop);
	
	for ( int i = 0; i < neighbors.size(); i++) {
          if ( neighbors[i] == last_hop ) {
	    BRN_DEBUG("Delete Src %s", neighbors[i].unparse().c_str());
	    neighbors.erase(neighbors.begin() + i);
	    break;
	  }
	}

        if (neighbors.size() == 0) {
          BRN_DEBUG("* UnicastFlooding: We have only weak or no neighbors; keep broadcast!");
          output(0).push(p_in);
          return;
        }
      }
    }
    
    bool rewrite = false;	

    switch (_cand_selection_strategy) {
      case UNICAST_FLOODING_NO_REWRITE: // no rewriting; keep as broadcast
        break;
      case 1:                           // algo 1 - choose the neighbor with the largest neighborhood minus our own neighborhood
        rewrite = algorithm_1(next_hop, neighbors);
        break;
      case 2:                           // algo 2 - choose the neighbor with the highest number of neighbors not covered by any other neighbor of the other neighbors
        rewrite = algorithm_2(next_hop, neighbors);
        break;
      case 3:                           // algo 3 - choose the neighbor not covered by any other neighbor
        rewrite = algorithm_3(next_hop, neighbors);
        if ( ! rewrite )
          rewrite = algorithm_1(next_hop, neighbors);
        break;
      case 4: {                         // algo 4 - choose worst neighbor (metric)
        int worst_n = findWorst(*me, neighbors);
        if ( worst_n != -1 ) {
          rewrite = true;  
          next_hop = neighbors[worst_n];
        }}
        break;
      case UNICAST_FLOODING_STATIC_REWRITE: // static rewriting
        BRN_DEBUG("Rewrite to static mac");
        rewrite = true;
        next_hop = static_dst_mac;
        break;
      case UNICAST_FLOODING_ALL_UNICAST: // static rewriting
        BRN_DEBUG("Send unicast to all neighbours");
        rewrite = true;
        next_hop = static_dst_mac;
        break;
      default:
        BRN_WARN("* UnicastFlooding: Unknown candidate selection strategy; keep as broadcast.");
    }
    
    if ( rewrite ) {
      memcpy(ether->ether_dhost, next_hop.data() , 6);
      last_unicast_used = next_hop;
      BRN_DEBUG("* UnicastFlooding: Destination address rewrote to %s", next_hop.unparse().c_str());
      no_rewrites++;
    }
    output(0).push(p_in);
  }

  return;
}

//-----------------------------------------------------------------------------
// Algorithms
//-----------------------------------------------------------------------------

bool
UnicastFlooding::algorithm_1(EtherAddress &next_hop, Vector<EtherAddress> &neighbors)
{
  bool rewrite = false;

  // search for the best nb
  int best_new_nb_cnt = -1;
  EtherAddress best_nb;
  for( int n_i = 0; n_i < neighbors.size(); n_i++) {
    // estimate neighborhood of this neighbor
    Vector<EtherAddress> nb_neighbors;
    get_filtered_neighbors(neighbors[n_i], nb_neighbors);

    // subtract from nb_neighbors all nbs already covered by myself and estimate cardinality of this set
    int new_nb_cnt = subtract_and_cnt(nb_neighbors, neighbors);

    if (new_nb_cnt > best_new_nb_cnt) {
      best_new_nb_cnt = new_nb_cnt;
      best_nb = neighbors[n_i];
    }
  }

  // check if we found a candidate and rewrite packet
  if (best_new_nb_cnt > -1) {
    rewrite = true;  // rewrite address	
    next_hop = best_nb;
  }

  return rewrite;
}

bool
UnicastFlooding::algorithm_2(EtherAddress &next_hop, Vector<EtherAddress> &neighbors)
{
  bool rewrite = false; 
 
  // search for the best nb
  int best_metric = -1;
  EtherAddress best_nb;
  
  for( int n_i = 0; n_i < neighbors.size(); n_i++) { // iterate over all my neighbors
    // estimate neighborhood of this neighbor
    Vector<EtherAddress> nb_neighbors; // the neighbors of my neighbors
    get_filtered_neighbors(neighbors[n_i], nb_neighbors);

    // estimate the neighbors of all other neighbors except neighbors[n_i]
    Vector<EtherAddress> nbnbUnion;
    // add 1-hop nbs
    addAll(neighbors, nbnbUnion);
    for( int n_j = 0; n_j < neighbors.size(); n_j++) { // iterate over all my neighbors
      if (n_i == n_j) continue;

      // estimate neighborhood of this neighbor
      Vector<EtherAddress> nb_neighbors2; // the neighbors of my neighbors
      get_filtered_neighbors(neighbors[n_j], nb_neighbors2);
      // union
      addAll(nb_neighbors2, nbnbUnion);
    }

    // remove from nb_neighbors all nodes from nbnbUnion
    int metric = subtract_and_cnt(nb_neighbors, nbnbUnion);
    
    if (metric > best_metric) {
      best_metric = metric;
      best_nb = neighbors[n_i];
    }
  }

  // check if we found a candidate and rewrite packet
  if (best_metric > -1) {
    rewrite = true;  // rewrite address	
    next_hop = best_nb;
  }

  return rewrite;
}

bool
UnicastFlooding::algorithm_3(EtherAddress &next_hop, Vector<EtherAddress> &neighbors)
{
  uint8_t e[] = { 0,0,0,0,1,2};
  bool rewrite = false; 

  Vector<EtherAddress> nbnbUnion;

  for( int n_i = 0; n_i < neighbors.size(); n_i++) { // iterate over all my neighbors
    // estimate neighborhood of this neighbor
    Vector<EtherAddress> nb_neighbors; // the neighbors of my neighbors
    get_filtered_neighbors(neighbors[n_i], nb_neighbors);

    // estimate the neighbors of all other neighbors except neighbors[n_i]
    // add 1-hop nbs
    addAll(nb_neighbors, nbnbUnion);
  }

  if ( *(_me->getMasterAddress()) == EtherAddress(e) ) {
    click_chatter("Neigh");
    print_vector(neighbors);
    
    click_chatter("Union");
    print_vector(nbnbUnion);
  }
  
  // search in my 1-hop neighborhood
  Vector<EtherAddress> candidates;

  for( int n_j = 0; n_j < neighbors.size(); n_j++) { // iterate over all my neighbors
    int n_i = 0;
    for(; n_i < nbnbUnion.size(); n_i++)             // iterate over all my 2-hop neighbors
      if ( neighbors[n_j] == nbnbUnion[n_i] ) break;

    if ( n_i == nbnbUnion.size() )
      candidates.push_back(neighbors[n_j]);
  }

  if ( *(_me->getMasterAddress()) == EtherAddress(e) ) {
    BRN_ERROR("CANDSize: %d %d %d", candidates.size(), neighbors.size(), nbnbUnion.size());
  }

  if ( candidates.size() == 0 ) return false;
  if ( candidates.size() == 1 ) {
    rewrite = true;
    next_hop = candidates[0];
  } else {
    rewrite = true;    
    next_hop = candidates[findWorst(*(_me->getMasterAddress()), candidates)];
  }
  
  if ( *(_me->getMasterAddress()) == EtherAddress(e) ) {
    BRN_ERROR("NEXT: %s",next_hop.unparse().c_str());
  }
  
  return rewrite;
}

//-----------------------------------------------------------------------------
// Helper methods
//-----------------------------------------------------------------------------

void
UnicastFlooding::get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out)
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
UnicastFlooding::filter_bad_one_hop_neighbors(const EtherAddress &node, Vector<EtherAddress> &neighbors, Vector<EtherAddress> &filtered_neighbors)
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
UnicastFlooding::filter_known_one_hop_neighbors(const EtherAddress &node, Vector<EtherAddress> &neighbors, Vector<EtherAddress> &filtered_neighbors)
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
UnicastFlooding::subtract_and_cnt(const Vector<EtherAddress> &s1, const Vector<EtherAddress> &s2)
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
UnicastFlooding::addAll(const Vector<EtherAddress> &newS, Vector<EtherAddress> &all)
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
UnicastFlooding::findWorst(const EtherAddress &src, Vector<EtherAddress> &neighbors)
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

enum { H_REWRITE_STATS, H_REWRITE_STRATEGY, H_REWRITE_STATIC_MAC };

static String
read_param(Element *e, void *thunk)
{
  UnicastFlooding *rewriter = (UnicastFlooding *)e;
  StringAccum sa;

  switch ((uintptr_t) thunk)
  {
    case H_REWRITE_STATS :
    {
      sa << "<unicast_rewriter node=\"" << rewriter->get_node_name() << "\" strategy=\"" << rewriter->get_strategy() << "\" static_mac=\"";
      sa << rewriter->get_static_mac()->unparse() << "\" last_rewrite=\"" << rewriter->last_unicast_used;
      sa << "\" no_rewrites=\"" << rewriter->no_rewrites << "\" >\n</unicast_rewriter>\n";
      break;
    }
    case H_REWRITE_STRATEGY :
    {
      sa << rewriter->get_strategy();
      break;
    }
    case H_REWRITE_STATIC_MAC :
    {
      sa << rewriter->get_static_mac()->unparse();
      break;
    }
    default: {
      return String();
    }
  }

  return sa.take_string();
}

static int 
write_param(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  UnicastFlooding *rewriter = (UnicastFlooding *)e;
  String s = cp_uncomment(in_s);

  switch((intptr_t)vparam) {
    case H_REWRITE_STRATEGY: {
      Vector<String> args;
      cp_spacevec(s, args);

      int strategy;
      cp_integer(args[0], &strategy);

      rewriter->set_strategy(strategy);
      break;
    }
    case H_REWRITE_STATIC_MAC: {
      Vector<String> args;
      cp_spacevec(s, args);

      EtherAddress mac;
      cp_ethernet_address(args[0], &mac);

      rewriter->set_static_mac(&mac);
      break;
    }
  }

  return 0;
}

void
UnicastFlooding::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("strategy", read_param , (void *)H_REWRITE_STRATEGY);
  add_read_handler("stats", read_param , (void *)H_REWRITE_STATS);
  add_read_handler("mac", read_param , (void *)H_REWRITE_STATIC_MAC);

  add_write_handler("strategy", write_param, (void *)H_REWRITE_STRATEGY);
  add_write_handler("mac", write_param, (void *)H_REWRITE_STATIC_MAC);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(UnicastFlooding)
