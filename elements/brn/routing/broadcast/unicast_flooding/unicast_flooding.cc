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
 * R. Sombrutzki
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

UnicastFlooding::UnicastFlooding():
  _me(NULL),
  _flooding(NULL),
  _fhelper(NULL),
  _flooding_db(NULL),
  _cand_selection_strategy(0),
  _pre_selection_mode(UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED),
  _ucast_peer_metric(0),
  _ucast_per_node_limit(0),
  _reject_on_empty_cs(true),
  _force_responsibility(false),
  _use_assign_info(false),
  _fix_candidate_set(false),
  _self_pdr(UNICAST_FLOODING_DIJKSTRA_PDR),
  _self_pdr_steps(0),
  _self_pdr_limit(UNICAST_FLOODING_DIJKSTRA_PDR),
  _cnt_rewrites(0),
  _cnt_bcasts(0),
  _cnt_bcasts_empty_cs(0),
  _cnt_reject_on_empty_cs(0)
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
  String pdr_config = "";

  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "FLOODING", cpkP+cpkM, cpElement, &_flooding,
      "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
      "FLOODINGDB", cpkP+cpkM, cpElement, &_flooding_db,
      "PRESELECTIONSTRATEGY", cpkP, cpInteger, &_pre_selection_mode,
      "REJECTONEMPTYCS", cpkP, cpBool, &_reject_on_empty_cs,
      "CANDSELECTIONSTRATEGY", cpkP, cpInteger, &_cand_selection_strategy,
      "UCASTPEERMETRIC", cpkP, cpInteger, &_ucast_peer_metric,
      "FORCERESPONSIBILITY", cpkP, cpBool, &_force_responsibility,
      "USEASSIGNINFO", cpkP, cpBool, &_use_assign_info,
      "STATIC_DST", cpkP, cpEtherAddress, &static_dst_mac,
      "FIXCS", cpkP, cpBool, &_fix_candidate_set,
      "PDRCONFIG", cpkP, cpString, &pdr_config,
      "UCASTPERNODELIMIT", cpkP, cpInteger, &_ucast_per_node_limit,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if ((_cand_selection_strategy == UNICAST_FLOODING_STATIC_REWRITE) &&
      (static_dst_mac.is_broadcast())) _cand_selection_strategy = UNICAST_FLOODING_NO_REWRITE;

  String pdr_config_unc = cp_uncomment(pdr_config);

  Vector<String> args;
  cp_spacevec(pdr_config_unc, args);

  if ( args.size() != 0 ) {
    cp_integer(args[0], &_self_pdr);
    if ( args.size() == 3 ) {
      cp_integer(args[1], &_self_pdr_steps);
      cp_integer(args[2], &_self_pdr_limit);
    }
  }

  return 0;
}

int
UnicastFlooding::initialize(ErrorHandler *)
{
  click_brn_srandom();

  last_unicast_used = EtherAddress::make_broadcast();

  return 0;
}

void
UnicastFlooding::uninitialize()
{
  //cleanup
}


void
UnicastFlooding::push(int, Packet *p)
{
  //port 0: transmit to other brn nodes; rewrite from broadcast to unicast
  if (Packet *q = smaction(p, true)) {

    if ( q != NULL ) {
      BRN_DEBUG("Set Last: %s %s %d",_last_tx_dst_ea.unparse().c_str(), _last_tx_src_ea.unparse().c_str(), _last_tx_bcast_id);
      _flooding->set_last_tx(_last_tx_dst_ea, _last_tx_src_ea, _last_tx_bcast_id);
    }

    output(0).push(q);
  }
}

Packet *
UnicastFlooding::pull(int)
{
  Packet *p = NULL;

  do {
    if ((p = input(0).pull()) == NULL) break;  //no packet from upper layer, so finished
    p = smaction(p, false);                    //handle new packet
  } while (p == NULL);                         //repeat 'til we have a packet for lower layer

  if ( p != NULL ) {
    BRN_DEBUG("Set Last: %s %s %d",_last_tx_dst_ea.unparse().c_str(), _last_tx_src_ea.unparse().c_str(), _last_tx_bcast_id);
    _flooding->set_last_tx(_last_tx_dst_ea, _last_tx_src_ea, _last_tx_bcast_id);
  }

  return p;
}

/*
 * process an incoming broadcast/unicast packet:
 * in[0] : // received from other brn node; rewrite from unicast to broadcast
 * in[1] : // transmit to other brn nodes; rewrite from broadcast to unicast
 * out: the rewroted or restored packet
 *
 * return NULL if packet was rejected (and push the packet to 1
 * function should recall smaction with another packet
 *
 */
Packet *
UnicastFlooding::smaction(Packet *p_in, bool /*is_push*/)
{
  // next hop must be a broadcast
  EtherAddress next_hop = BRNPacketAnno::dst_ether_anno(p_in);
  assert(next_hop.is_broadcast());

  const EtherAddress *me;
  Vector<EtherAddress> candidate_set;
  candidate_set.clear();

  //filter packet src
  struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)&(p_in->data()[sizeof(struct click_brn)]);
  EtherAddress src = EtherAddress((uint8_t*)&(p_in->data()[sizeof(struct click_brn) + sizeof(struct click_brn_bcast) + bcast_header->extra_data_size])); //src follows header

  uint16_t bcast_id = ntohs(bcast_header->bcast_id);
  //clear responseflag for the case that broadcast is used
  bcast_header->flags &= ~(BCAST_HEADER_FLAGS_FORCE_DST & BCAST_HEADER_FLAGS_REJECT_ON_EMPTY_CS &
                           BCAST_HEADER_FLAGS_REJECT_WITH_ASSIGN & BCAST_HEADER_FLAGS_REJECT_DUE_STOPPED);

  uint16_t assigned_nodes = 0;

  BroadcastNode *bcn = _flooding_db->get_broadcast_node(&src);

   int min_tx_count;
   int min_tx_count_index;

  /* check wether bcn-pkt mark as stopped (txabort) */
  if ( bcn->is_stopped(bcast_id) ) {
    bcast_header->flags |= BCAST_HEADER_FLAGS_REJECT_DUE_STOPPED;
    output(1).push(p_in);
    return NULL;
  }

  if ( _cand_selection_strategy == UNICAST_FLOODING_STATIC_REWRITE ) {
    candidate_set.push_back(static_dst_mac);
  } else if ( _cand_selection_strategy != UNICAST_FLOODING_NO_REWRITE ) {

    /**
      *
      * PRE SELECTION
      *
      * Candidate Selection
      * 0. All neighbours                                                        (All nodes)
      * 1. Neighbour (best set) which covers all n-hop neighbours (use dijkstra) (Strong connected)
      * 2. Child only: nodes, which covers other (2-n)-hop nodes alone. There is no other node in my neighbourhood
      *
      * In all cases all nodes which have already the message are removed.
      *
      * */

    //src mac
    me = _me->getMasterAddress();

    //filter packet src
    BRN_DEBUG("Src %s id: %d",src.unparse().c_str(), bcast_id);

    //get all known nodes
    struct BroadcastNode::flooding_node_info *last_nodes;
    uint32_t last_nodes_size;
    last_nodes = _flooding_db->get_node_infos(&src, bcast_id, &last_nodes_size);

    Vector<EtherAddress> known_neighbors;

    /**
     * First, check for responsible nodes
     * If we have such nodes than we already have a candidate set
     */

    for ( uint32_t j = 0; j < last_nodes_size; j++ ) {                                       //add node to candidate set if
      if ( ((last_nodes[j].flags & FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY) != 0) &&         //1. i'm responsible (due unicast before or fix set)
           ((last_nodes[j].flags & FLOODING_NODE_INFO_FLAGS_FINISHED) == 0)) {               //2. is not acked
        candidate_set.push_back(EtherAddress(last_nodes[j].etheraddr));
      } else {
        if ((last_nodes[j].flags & FLOODING_NODE_INFO_FLAGS_FINISHED_FOR_ME) != 0)           //is known as acked or foreign response
          known_neighbors.push_back(EtherAddress(last_nodes[j].etheraddr));
        /*else {
          BRN_DEBUG("Not finished: %s" , EtherAddress(last_nodes[j].etheraddr).unparse().c_str());
        }*/
      }
    }

    //BRN_DEBUG("Known neighbours: %d CS: %d", known_neighbors.size(), candidate_set.size());

    if ( _use_assign_info ) {
      /**
       * a nother node try to reach the node
       */
      BRN_DEBUG("Assigned node size: %d", last_nodes_size);
      for ( uint32_t j = 0; j < last_nodes_size; j++ ) {                           //add node to known_nodes if
        if ((last_nodes[j].flags & FLOODING_NODE_INFO_FLAGS_IS_ASSIGNED_NODE) != 0) { //1. it is assigned
          //TODO: Debug: check whether node is already in known_neighbours
          //BRN_DEBUG("Add assigned node");
          known_neighbors.push_back(EtherAddress(last_nodes[j].etheraddr));
          assigned_nodes++;
        }
      }
    }

    /**
     * all_nodes get all_nodes except last_nodes
     *
     * _fix_target_set is true if the broadcast algorith has a fix target set (like mpr or mst)
     */
    if ( ! bcn->_fix_target_set ) {

      /** we use all nodes (neighbors) or we have a candidate set (due responsibility) */
      if ( (_pre_selection_mode == UNICAST_FLOODING_PRESELECTION_ALL_NODES) ||
           ( candidate_set.size() != 0) ) {

        /* if we have no cands take all neighbors */
        if ( candidate_set.size() == 0 ) _fhelper->get_filtered_neighbors(*me, candidate_set);

        /* delete all known neighbours */ /* could improve performance if necessary: sort both vectors before and create new array instead of using erase */
        for ( int i = candidate_set.size()-1; i >= 0; i--) {
          for ( int j = 0; j < known_neighbors.size(); j++) {
            if ( candidate_set[i] == known_neighbors[j] ) {
              candidate_set.erase(candidate_set.begin() + i);
              break;
            }
          }
        }

      } else {

        //get local graph info
        NetworkGraph net_graph;

        uint32_t final_pre_selection_mode = _pre_selection_mode;

        //init with startpdr
        uint32_t selfpdr = _self_pdr;

        if ( _self_pdr_steps > 0 ) {
          //got number of real forwards
          //TODO: use only number of mac-forwards (sent-count)
          uint32_t no_fwds = _flooding_db->forward_done_cnt(&src, bcast_id);

          selfpdr = MIN(selfpdr + (no_fwds * _self_pdr_steps), _self_pdr_limit);
        }

        do {
          if ( final_pre_selection_mode == UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED ) {
            _fhelper->get_local_graph(*me, known_neighbors, net_graph, 2, selfpdr);
          } else if ( final_pre_selection_mode == UNICAST_FLOODING_PRESELECTION_CHILD_ONLY ) {
            _fhelper->get_local_childs(*me, net_graph, 3);

            for ( int i = 0; i < known_neighbors.size(); i++) {
              _fhelper->remove_node(known_neighbors[i], net_graph);
            }

            BRN_DEBUG("NET_GRAPH size: %d",net_graph.size());
            _fhelper->print_vector(net_graph.nml);

          }

          //get candidateset
          _fhelper->get_candidate_set(net_graph, candidate_set);
          BRN_DEBUG("Candset size: %d",candidate_set.size());

          //clear unused stuff
          _fhelper->clear_graph(net_graph);

          if ( candidate_set.size() == 0 ) {
            if ( final_pre_selection_mode == UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED ) {
              break;
            } else if ( final_pre_selection_mode == UNICAST_FLOODING_PRESELECTION_CHILD_ONLY ) {
              /* We have to switch to avoid, that no node sends a packet in e.g. a full connected network */
              BRN_DEBUG("Preselection: Switch to UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED");
              final_pre_selection_mode = UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED;
            }
          }

        } while (candidate_set.empty());


        /* We have a candidate set. We fix it now*/
        if ( _fix_candidate_set && (!candidate_set.empty()) ) {
          for (Vector<EtherAddress>::iterator i = candidate_set.begin(); i != candidate_set.end(); ++i) {
            _flooding_db->add_node_info(&src, bcast_id, i, false, false, true, false);
            //BRN_DEBUG("Add node %s %d %s", i->unparse().c_str(), bcast_id, src.unparse().c_str());
          }
        }

        /* print result */
        _fhelper->print_vector(candidate_set);
      }

    }  //end fix_target

    //clear known neighbours1
    known_neighbors.clear();

    /**
     * Finished first step of preselection
     */

    if (candidate_set.size() == 0) {
      if ( _reject_on_empty_cs ) {
        BRN_DEBUG("We have only weak or no neighbors. Reject!");
        _cnt_reject_on_empty_cs++;
        _flooding_db->clear_assigned_nodes(&src, bcast_id);                     //clear all assignment (temporaly mark as "the node has the paket"
        bcast_header->flags |= BCAST_HEADER_FLAGS_REJECT_ON_EMPTY_CS;           //reject and assign nodes are part of decision
        if ( assigned_nodes > 0 ) bcast_header->flags |= BCAST_HEADER_FLAGS_REJECT_WITH_ASSIGN;
        output(1).push(p_in);
        return NULL;
      }

      BRN_DEBUG("We have only weak or no neighbors. Keep Bcast!");
      _cnt_bcasts_empty_cs++;
    }
  } // end of !"no rewrite"


  /**
    *
    * FINAL SELECTION
    *
    * */

  if ( candidate_set.size() > 1 ) {
    switch (_cand_selection_strategy) {

      case UNICAST_FLOODING_BCAST_WITH_PRECHECK:                       //Do nothing (just reject if no node left (see up))
        break;

      case UNICAST_FLOODING_ALL_UNICAST:    // static rewriting

        BRN_DEBUG("Send unicast to all neighbours");

        min_tx_count = _flooding_db->get_unicast_tx_count(&src, bcast_id, &candidate_set[0]);
        min_tx_count_index = 0;

        for ( int i = 1; i < candidate_set.size()-1; i++) {
          int node_tx_count = _flooding_db->get_unicast_tx_count(&src, bcast_id, &candidate_set[i]);
          if (node_tx_count < min_tx_count) {
            min_tx_count = node_tx_count;
            min_tx_count_index = i;
          }
        }
        next_hop = candidate_set[min_tx_count_index];
        break;

      case UNICAST_FLOODING_TAKE_BEST:     // take node with highest link quality
        next_hop = candidate_set[_fhelper->find_best(*me, candidate_set)];
        break;

      case UNICAST_FLOODING_TAKE_WORST:     // take node with lowest link quality
        next_hop = candidate_set[_fhelper->find_worst(*me, candidate_set)];
        break;

      case UNICAST_FLOODING_MOST_NEIGHBOURS:                           // algo 1 - choose the neighbor with the largest neighborhood minus our own neighborhood
        next_hop = algorithm_most_neighbours(candidate_set, 2);
        break;

      case UNICAST_FLOODING_PRIO_LOW_BENEFIT:                           //
        next_hop = algorithm_less_neighbours(candidate_set, 2);
        break;

      case UNICAST_FLOODING_RANDOM:                                     //
        next_hop = candidate_set[click_random() % candidate_set.size()];
        break;

      default:
        BRN_WARN("* UnicastFlooding: Unknown candidate selection strategy; keep as broadcast.");
    }
  } else {
    if ((candidate_set.size() == 1) && (_cand_selection_strategy != UNICAST_FLOODING_BCAST_WITH_PRECHECK))
      next_hop = candidate_set[0];
  }

  if ( next_hop.is_broadcast() ) {
    _cnt_bcasts++;
  } else {
    //TODO: clear assign nodes? or wait with clear until no nodes is left?
    BRNPacketAnno::set_dst_ether_anno(p_in, next_hop);
    last_unicast_used = next_hop;

    BRN_DEBUG("* UnicastFlooding: Destination address rewrote to %s", next_hop.unparse().c_str());
    _cnt_rewrites++;

    _flooding_db->inc_unicast_tx_count(&src, ntohs(bcast_header->bcast_id), &next_hop);

    if (_force_responsibility) {
      _flooding_db->set_responsibility_target(&src, ntohs(bcast_header->bcast_id), &next_hop);
      bcast_header->flags |= BCAST_HEADER_FLAGS_FORCE_DST;
    }
  }

  //BRN_ERROR("Me: %s Dst: %s CSS: %d id: %d m: %d cm: %d",
  //                                           me->unparse().c_str(), next_hop.unparse().c_str(),
  //                                           candidate_set.size(),bcast_id,
  //                                           _fhelper->_link_table->get_link_pdr(*me,next_hop),
  //                                           _fhelper->_cnmlmap.find(*me)->get_metric(next_hop));
  add_rewrite(&src, bcast_id, &next_hop);

  _flooding_db->clear_assigned_nodes(&src, bcast_id);

  _last_tx_dst_ea = next_hop;
  _last_tx_src_ea = src;
  _last_tx_bcast_id = bcast_id;

  candidate_set.clear();

  return p_in;
}

//-----------------------------------------------------------------------------
// Algorithms
//-----------------------------------------------------------------------------

EtherAddress
UnicastFlooding::algorithm_most_neighbours(Vector<EtherAddress> &neighbors, int hops)
{
  return algorithm_neighbours(neighbors, hops, true);
}

EtherAddress
UnicastFlooding::algorithm_less_neighbours(Vector<EtherAddress> &neighbors, int hops)
{
  return algorithm_neighbours(neighbors, hops, false);
}

EtherAddress
UnicastFlooding::algorithm_neighbours(Vector<EtherAddress> &neighbors, int hops, bool most)
{
  const EtherAddress *me = _me->getMasterAddress();

  //local Graph
  NetworkGraph local_graph;
  _fhelper->init_graph(*me, local_graph, 100);
  _fhelper->get_graph(local_graph, hops, 100);

  Vector<EtherAddress> local_neighbours;
  _fhelper->get_filtered_neighbors(*me, local_neighbours);

  // search for the best nb
  int32_t best_new_metric = -1;
  EtherAddress best_nb;

  //start node is blacklist node
  HashMap<EtherAddress, EtherAddress> blacklist;
  blacklist.insert(*me, *me);

  NetworkGraph *ng_list = new NetworkGraph[neighbors.size()];

  /**
   * get neighbours of neighbours without start node
   * x 2-hop neighbourhood
   *
   * (nb(nb(x)\{c}) U nb(x))
   * 
   */
  for ( int x = 0; x < neighbors.size(); x++ ) {
    _fhelper->init_graph(neighbors[x], ng_list[x], 100);
    _fhelper->get_graph(ng_list[x], hops, 100, blacklist);
  }

  for( int x = 0; x < neighbors.size(); x++) {
    //Estimate size etc...

    /**
     * TODO: B1.1 & B1.2: warum nur durch grösse der einhap nachbarschaft teilen und nicht 2 hop?
     *
     * B1.1. | nb(c) \ (nb(nb(x)\{c}) U nb(x)) | / | nb(c) n nb(x) |
     * B1.2. | nb(c) \ (nb(nb(x)\{c}) U nb(x)) | / | (nb(nb(c)) u nb(c)) n nb(x) |
     * B1.3. | nb(c) \ (nb(nb(x)\{c}) U nb(x)) |
     * 
     * B2.1. | nb(c) \ nb(x) | / | nb(c) n nb(x) |
     * B2.2. | nb(c) \ nb(x) | / | (nb(nb(c)) U nb(c)) n nb(x) |
     * B2.3. | nb(c) \ nb(x) |
     * 
     **/

    /** nb(c) \ (nb(nb(x)\{c}) U nb(x)) **/
    /* Neighbours only coverd by c */
    uint32_t foreign_exclusive_nb_hood = _fhelper->graph_diff_size(local_neighbours, ng_list[x]);

    /** nb(c) \ nb(x) **/
    Vector<EtherAddress> x_neighbours;
    _fhelper->get_filtered_neighbors(neighbors[x], x_neighbours); //nb(x)

    uint32_t foreign_exclusive_nb = _fhelper->graph_diff_size(local_neighbours, x_neighbours);

    /** nb(c) n nb(x)  **/
    uint32_t cut_nb_c_nb_x = _fhelper->graph_cut_size(x_neighbours, local_neighbours);

    /** (nb(nb(c)) u nb(c)) n nb(x) **/
    uint32_t cut_local_graph_nb_x = _fhelper->graph_cut_size(x_neighbours, local_graph);

    int32_t new_metric = 0;

    switch (_ucast_peer_metric) {
      case 0: new_metric = (100 * foreign_exclusive_nb_hood) / (cut_nb_c_nb_x + 1);
              break;
      case 1: new_metric = (100 * foreign_exclusive_nb_hood) / (cut_local_graph_nb_x + 1);
              break;
      case 2: new_metric = (100 * foreign_exclusive_nb_hood);
              break;
      case 3: new_metric = (100 * foreign_exclusive_nb) / (cut_nb_c_nb_x + 1);
              break;
      case 4: new_metric = (100 * foreign_exclusive_nb) / (cut_local_graph_nb_x + 1);
              break;
      case 5: new_metric = (100 * foreign_exclusive_nb);
              break;
    }

    BRN_DEBUG("MostNeighbours: NODE: %s Value: %d", neighbors[x].unparse().c_str(), new_metric);
    if (most) {
      if (new_metric > best_new_metric) {
        best_new_metric = new_metric;
        best_nb = neighbors[x];
      }
    } else {
      if ((new_metric < best_new_metric) || (x == 0)) {
        best_new_metric = new_metric;
        best_nb = neighbors[x];
      }
    }

    x_neighbours.clear();
    _fhelper->clear_graph(ng_list[x]);
  }


  local_neighbours.clear();
  _fhelper->clear_graph(local_graph);
  delete[] ng_list;

  BRN_DEBUG("Best: %s Metric: %d",best_nb.unparse().c_str(), best_new_metric);

  // check if we found a candidate and rewrite packet
  if (best_new_metric > -1) return best_nb;

  return brn_etheraddress_broadcast;
}

void
UnicastFlooding::add_rewrite(EtherAddress */*src*/, uint16_t /*id*/, EtherAddress *target)
{
  uint32_t *cnt;
  if ( (cnt = rewrite_cnt_map.findp(*target)) == NULL ) {
    rewrite_cnt_map.insert(*target,1);
  } else {
    *cnt = *cnt + 1;
  }
}

String
UnicastFlooding::get_strategy_string(uint32_t id)
{
  switch (id) {
    case UNICAST_FLOODING_NO_REWRITE: return "no_rewrite";
    case UNICAST_FLOODING_STATIC_REWRITE: return "static_rewrite";
    case UNICAST_FLOODING_ALL_UNICAST: return "all_unicast";
    case UNICAST_FLOODING_TAKE_WORST: return "take_worst";
    case UNICAST_FLOODING_MOST_NEIGHBOURS: return "most_neighbours";
    case UNICAST_FLOODING_BCAST_WITH_PRECHECK: return "bcast_with_precheck";
  }

  return "unknown";
}

String
UnicastFlooding::get_preselection_string(uint32_t id)
{
  switch (id) {
    case UNICAST_FLOODING_PRESELECTION_ALL_NODES: return "all_node";
    case UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED: return "strong_connected";
    case UNICAST_FLOODING_PRESELECTION_CHILD_ONLY: return "child_only";
  }

  return "unknown";
}
//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum { H_REWRITE_STATS, H_REWRITE_STRATEGY, H_REWRITE_PRESELECTION, H_REWRITE_STATIC_MAC };

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
      sa << "\" strategy_string=\"" << rewriter->get_strategy_string(rewriter->get_strategy()) << "\" preselection=\"";
      sa << rewriter->get_preselection() << "\" preselection_string=\"" << rewriter->get_preselection_string(rewriter->get_preselection()); 
      sa << "\" ucast_peer_metric=\"" << rewriter->get_ucast_peer_metric() << "\" reject_on_empty_cs=\"" << (int)(rewriter->get_reject_on_empty_cs()?1:0);
      sa << "\" fix_cs=\"" << (int)(rewriter->_fix_candidate_set?1:0) << "\" force_response=\"" << (int)(rewriter->_force_responsibility?1:0);
      sa << "\" use_assign=\"" << (int)(rewriter->_use_assign_info?1:0);
      sa << "\" static_mac=\"" << rewriter->get_static_mac()->unparse() << "\" last_rewrite=\"" << rewriter->last_unicast_used;
      sa << "\" rewrites=\"" << rewriter->_cnt_rewrites << "\" bcast=\"" << rewriter->_cnt_bcasts;
      sa << "\" empty_cs_reject=\"" << rewriter->_cnt_reject_on_empty_cs;
      sa << "\" empty_cs_bcast=\"" << rewriter->_cnt_bcasts_empty_cs << "\" >\n";

      sa << "\t<rewrite_cnt targets=\"" << rewriter->rewrite_cnt_map.size() << "\" >\n";

      for ( UnicastFlooding::TargetRewriteCntMapIter trcm = rewriter->rewrite_cnt_map.begin(); trcm.live(); ++trcm) {
        uint32_t cnt = trcm.value();
        EtherAddress addr = trcm.key();

        sa << "\t\t<rewrite_cnt_node node=\"" << addr.unparse() << "\" cnt=\"" << cnt << "\" />\n";
      }

      sa << "\t</rewrite_cnt>\n";

      sa << "</unicast_rewriter>\n";
      break;
    }
    case H_REWRITE_STRATEGY :
    {
      sa << rewriter->get_strategy();
      break;
    }
    case H_REWRITE_PRESELECTION :
    {
      sa << rewriter->get_preselection();
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
    case H_REWRITE_PRESELECTION:
    case H_REWRITE_STRATEGY: {
      Vector<String> args;
      cp_spacevec(s, args);

      int arg_int;
      cp_integer(args[0], &arg_int);

      if ( (intptr_t)vparam == H_REWRITE_STRATEGY )
        rewriter->set_strategy(arg_int);
      else
	rewriter->set_preselection(arg_int);
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
  add_read_handler("preselection", read_param , (void *)H_REWRITE_PRESELECTION);
  add_read_handler("stats", read_param , (void *)H_REWRITE_STATS);
  add_read_handler("mac", read_param , (void *)H_REWRITE_STATIC_MAC);

  add_write_handler("strategy", write_param, (void *)H_REWRITE_STRATEGY);
  add_write_handler("preselection", write_param, (void *)H_REWRITE_PRESELECTION);
  add_write_handler("mac", write_param, (void *)H_REWRITE_STATIC_MAC);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(UnicastFlooding)
