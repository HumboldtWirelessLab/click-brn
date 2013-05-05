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
#include "../../../../userlevel/socket.hh"

CLICK_DECLS

UnicastFlooding::UnicastFlooding():
  _me(NULL),
  _flooding(NULL),
  _fhelper(NULL),
  _cand_selection_strategy(0),
  _cnt_rewrites(0),
  _cnt_bcasts(0)
{
  BRNElement::init();
  static_dst_mac = brn_etheraddress_broadcast;
}

UnicastFlooding::~UnicastFlooding()
{
}

int
UnicastFlooding::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "FLOODING", cpkP+cpkM, cpElement, &_flooding,
      "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
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
      
      me = _me->getMasterAddress();
      _fhelper->get_filtered_neighbors(*me, neighbors);
      _fhelper->filter_bad_one_hop_neighbors(*me, neighbors, filtered_neighbors);

      //filter packet src
      struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)&(p_in->data()[sizeof(struct click_ether) + sizeof(struct click_brn)]);
      EtherAddress src = EtherAddress((uint8_t*)&(p_in->data()[sizeof(click_ether) + sizeof(struct click_brn) + sizeof(struct click_brn_bcast) + bcast_header->extra_data_size])); //src follows header

      BRN_DEBUG("Src %s id: %d",src.unparse().c_str(), ntohs(bcast_header->bcast_id));
  
      struct Flooding::BroadcastNode::flooding_last_node *last_nodes;
      uint32_t last_nodes_size;
      
      last_nodes = _flooding->get_last_nodes(&src, ntohs(bcast_header->bcast_id), &last_nodes_size);
      
      for ( uint32_t j = 0; j < last_nodes_size; j++ ) {
        for ( int32_t i = neighbors.size()-1; i >= 0; i--) {
          if ( neighbors[i] == EtherAddress(last_nodes[j].etheraddr) ) {
            BRN_DEBUG("Delete Src %s", neighbors[i].unparse().c_str());
            neighbors.erase(neighbors.begin() + i);
          }
	}
      }

      if (neighbors.size() == 0) {
	BRN_DEBUG("* UnicastFlooding: We have only weak or no neighbors; keep broadcast!");
	output(0).push(p_in);
	return;
      }
    }
    
    bool rewrite = false;	

    switch (_cand_selection_strategy) {
      case UNICAST_FLOODING_NO_REWRITE:     // no rewriting; keep as broadcast
        break;
      case UNICAST_FLOODING_STATIC_REWRITE: // static rewriting
        BRN_DEBUG("Rewrite to static mac");
        rewrite = true;
        next_hop = static_dst_mac;
        break;
      case UNICAST_FLOODING_ALL_UNICAST:    // static rewriting
        BRN_DEBUG("Send unicast to all neighbours");
	
	for ( int i = 0; i < neighbors.size()-1; i++) {
	  Packet *p_copy = p_in->clone();
	  click_ether *c_ether = (click_ether *)p_copy->data();
	  memcpy(c_ether->ether_dhost, neighbors[i].data(), 6);
	  output(0).push(p_copy);
	}
        rewrite = true;
        next_hop = neighbors[neighbors.size()-1];
        break;
	
      case UNICAST_FLOODING_TAKE_WORST: {  // take node with lowest link quality
        int worst_n = _fhelper->findWorst(*me, neighbors);
        if ( worst_n != -1 ) {
          rewrite = true;  
          next_hop = neighbors[worst_n];
        } }
        break;
      case 4:                           // algo 1 - choose the neighbor with the largest neighborhood minus our own neighborhood
        rewrite = algorithm_1(next_hop, neighbors);
        break;
      case 5:                           // algo 2 - choose the neighbor with the highest number of neighbors not covered by any other neighbor of the other neighbors
        rewrite = algorithm_2(next_hop, neighbors);
        break;
      case 6:                           // algo 3 - choose the neighbor not covered by any other neighbor
        rewrite = algorithm_3(next_hop, neighbors);
        if ( ! rewrite ) {
          rewrite = algorithm_1(next_hop, neighbors);
	}
        break;
      default:
        BRN_WARN("* UnicastFlooding: Unknown candidate selection strategy; keep as broadcast.");
    }
    
    if ( rewrite ) {
      memcpy(ether->ether_dhost, next_hop.data(), 6);
      last_unicast_used = next_hop;
      BRN_DEBUG("* UnicastFlooding: Destination address rewrote to %s", next_hop.unparse().c_str());
      _cnt_rewrites++;
    } else {
      _cnt_bcasts++;
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
    _fhelper->get_filtered_neighbors(neighbors[n_i], nb_neighbors);

    // subtract from nb_neighbors all nbs already covered by myself and estimate cardinality of this set
    int new_nb_cnt = _fhelper->subtract_and_cnt(nb_neighbors, neighbors);

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
    _fhelper->get_filtered_neighbors(neighbors[n_i], nb_neighbors);

    // estimate the neighbors of all other neighbors except neighbors[n_i]
    Vector<EtherAddress> nbnbUnion;
    // add 1-hop nbs
    _fhelper->addAll(neighbors, nbnbUnion);
    for( int n_j = 0; n_j < neighbors.size(); n_j++) { // iterate over all my neighbors
      if (n_i == n_j) continue;

      // estimate neighborhood of this neighbor
      Vector<EtherAddress> nb_neighbors2; // the neighbors of my neighbors
      _fhelper->get_filtered_neighbors(neighbors[n_j], nb_neighbors2);
      // union
      _fhelper->addAll(nb_neighbors2, nbnbUnion);
    }

    // remove from nb_neighbors all nodes from nbnbUnion
    int metric = _fhelper->subtract_and_cnt(nb_neighbors, nbnbUnion);
    
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
    _fhelper->get_filtered_neighbors(neighbors[n_i], nb_neighbors);

    // estimate the neighbors of all other neighbors except neighbors[n_i]
    // add 1-hop nbs
    _fhelper->addAll(nb_neighbors, nbnbUnion);
  }

  if ( *(_me->getMasterAddress()) == EtherAddress(e) ) {
    click_chatter("Neigh");
    FloodingHelper::print_vector(neighbors);
    
    click_chatter("Union");
    FloodingHelper::print_vector(nbnbUnion);
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
    next_hop = candidates[_fhelper->findWorst(*(_me->getMasterAddress()), candidates)];
  }
  
  if ( *(_me->getMasterAddress()) == EtherAddress(e) ) {
    BRN_ERROR("NEXT: %s",next_hop.unparse().c_str());
  }
  
  return rewrite;
}


String
UnicastFlooding::get_strategy_string(uint32_t id)
{
  switch (id) {
    case UNICAST_FLOODING_NO_REWRITE: return "no_rewrite";
    case UNICAST_FLOODING_STATIC_REWRITE: return "static rewrite";
    case UNICAST_FLOODING_ALL_UNICAST: return "all_unicast";
    case UNICAST_FLOODING_TAKE_WORST: return "take_worst";
  }

    return "unknown"; 
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
      sa << "<unicast_rewriter node=\"" << rewriter->get_node_name() << "\" strategy=\"" << rewriter->get_strategy();
      sa << "\" strategy_string=\"" << rewriter->get_strategy_string(rewriter->get_strategy()) << "\" static_mac=\"";
      sa << rewriter->get_static_mac()->unparse() << "\" last_rewrite=\"" << rewriter->last_unicast_used;
      sa << "\" rewrites=\"" << rewriter->_cnt_rewrites << "\" bcast=\"" << rewriter->_cnt_bcasts <<"\" >\n</unicast_rewriter>\n";
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
