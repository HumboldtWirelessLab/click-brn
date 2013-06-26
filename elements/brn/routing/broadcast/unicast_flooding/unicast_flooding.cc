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
  _pre_selection_mode(UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED),
  _ucast_peer_metric(0),
  _reject_on_empty_cs(true),
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
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "FLOODING", cpkP+cpkM, cpElement, &_flooding,
      "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
      "PRESELECTIONSTRATEGY", cpkP, cpInteger, &_pre_selection_mode,
      "REJECTONEMPTYCS", cpkP, cpBool, &_reject_on_empty_cs,
      "CANDSELECTIONSTRATEGY", cpkP, cpInteger, &_cand_selection_strategy,
      "UCASTPEERMETRIC", cpkP, cpInteger, &_ucast_peer_metric,
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

  all_unicast_pkt_queue.clear();

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
  if (Packet *q = smaction(p, true)) output(0).push(q);
}

Packet *
UnicastFlooding::pull(int)
{
  Packet *p = NULL;

  if ( all_unicast_pkt_queue.size() > 0 ) {                          //first take packets from packet queue
    p = all_unicast_pkt_queue[0];                                    //take first
    all_unicast_pkt_queue.erase(all_unicast_pkt_queue.begin());      //clear
    return p;
  } else if (Packet *p = input(0).pull()) return smaction(p, false); //new packet

  return NULL;
}

/* 
 * process an incoming broadcast/unicast packet:
 * in[0] : // received from other brn node; rewrite from unicast to broadcast
 * in[1] : // transmit to other brn nodes; rewrite from broadcast to unicast
 * out: the rewroted or restored packet
 */
Packet *
UnicastFlooding::smaction(Packet *p_in, bool is_push)
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

  if ( (_cand_selection_strategy != UNICAST_FLOODING_STATIC_REWRITE) &&
        (_cand_selection_strategy != UNICAST_FLOODING_NO_REWRITE) ) {

    /**
      * 
      * PRE SELECTION
      * 
      * Candidate Selection
      * 1. All neighbours                                                        (All nodes)
      * 2. Neighbour (best set) which covers all n-hop neighbours (use dijkstra) (Strong connected)
      * 
      * In all cases all nodes which have already the message are removed.
      * 
      * */
    
    //src mac
    me = _me->getMasterAddress();

    //filter packet src
    BRN_DEBUG("Src %s id: %d",src.unparse().c_str(), ntohs(bcast_header->bcast_id));

    //get all known nodes
    struct Flooding::BroadcastNode::flooding_last_node *last_nodes;
    uint32_t last_nodes_size;
    last_nodes = _flooding->get_last_nodes(&src, ntohs(bcast_header->bcast_id), &last_nodes_size);
    
    Vector<EtherAddress> known_neighbors;

    for ( uint32_t j = 0; j < last_nodes_size; j++ ) {
      if ( memcmp(last_nodes[j].etheraddr,me->data(),6) != 0 ) {
        known_neighbors.push_back(EtherAddress(last_nodes[j].etheraddr));
      }
    }
    
    if ( _pre_selection_mode == UNICAST_FLOODING_PRESELECTION_ALL_NODES ) {
      
      _fhelper->get_filtered_neighbors(*me, candidate_set);
      
      for ( int i = candidate_set.size()-1; i >= 0; i--) {
        for ( int j = 0; j < known_neighbors.size(); j++) {
          if ( candidate_set[i] == known_neighbors[j] ) {
            candidate_set.erase(candidate_set.begin() + i);
            break;
          }
        }  
      }

      known_neighbors.clear();

    } else {

      //get local graph info
      NetworkGraph net_graph;

      uint32_t final_pre_selection_mode = _pre_selection_mode;

      do {
        if ( final_pre_selection_mode == UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED ) {
          _fhelper->get_local_graph(*me, known_neighbors, net_graph, 2, 110);
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
            BRN_DEBUG("Preselection: Switch to UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED");
            final_pre_selection_mode = UNICAST_FLOODING_PRESELECTION_STRONG_CONNECTED;
          } 
        }

      } while ( candidate_set.size() == 0 );

      //clear known neighbours1
      known_neighbors.clear();

      _fhelper->print_vector(candidate_set);
    }
      
    if (candidate_set.size() == 0) {
      if ( _reject_on_empty_cs ) {
        BRN_DEBUG("We have only weak or no neighbors. Reject!");
        _cnt_reject_on_empty_cs++;
        output(1).push(p_in);
        return NULL;
      }
      
      BRN_DEBUG("We have only weak or no neighbors. Keep Bcast!");
      _cnt_bcasts_empty_cs++;
      add_rewrite(&src, ntohs(bcast_header->bcast_id), &next_hop);
      return p_in;
    }
  } // end preselection strong connected
  
  
  /**
    * 
    * FINAL SELECTION
    * 
    * */
  bool rewrite = false;
  int worst_n;

  if ( candidate_set.size() > 1 ) { 
    switch (_cand_selection_strategy) {

      case UNICAST_FLOODING_ALL_UNICAST:    // static rewriting
  
        BRN_DEBUG("Send unicast to all neighbours");

        for ( int i = 0; i < candidate_set.size()-1; i++) {
          Packet *p_copy = p_in->clone()->uniqueify();

          BRNPacketAnno::set_dst_ether_anno(p_copy, candidate_set[i]);
  
          if ( src != *me ) _flooding->_flooding_fwd++;
          //TODO: inc src_fwd if i'm src of packet? 

          _flooding->forward_attempt(&src, ntohs(bcast_header->bcast_id));

          add_rewrite(&src, ntohs(bcast_header->bcast_id), &next_hop);
          
          if ( is_push ) output(0).push(p_copy);        //push element: push all packets
          else all_unicast_pkt_queue.push_back(p_copy); //pull element: store packets for next pull
        }
        rewrite = true;
        next_hop = candidate_set[candidate_set.size()-1];
        break;

      case UNICAST_FLOODING_TAKE_WORST:     // take node with lowest link quality
        worst_n = _fhelper->find_worst(*me, candidate_set);
        if ( worst_n != -1 ) {
          rewrite = true;  
          next_hop = candidate_set[worst_n];
        }
        break;

      case UNICAST_FLOODING_MOST_NEIGHBOURS:                           // algo 1 - choose the neighbor with the largest neighborhood minus our own neighborhood
        rewrite = algorithm_most_neighbours(next_hop, candidate_set, 2);
        break;

      default:
        BRN_WARN("* UnicastFlooding: Unknown candidate selection strategy; keep as broadcast.");
    }
  } else if ( candidate_set.size() == 1 ) {
    rewrite = true;
    next_hop = candidate_set[0];       
  } else { //candidate_set.size() == 0 -> no candidate
    switch (_cand_selection_strategy) {
      case UNICAST_FLOODING_NO_REWRITE:     // no rewriting; keep as broadcast
        break;
      case UNICAST_FLOODING_STATIC_REWRITE: // static rewriting
        BRN_DEBUG("Rewrite to static mac");
        rewrite = true;
        next_hop = static_dst_mac;
        break;
      default: //TODO: empty cs should never happended if neither UNICAST_FLOODING_NO_REWRITE nor UNICAST_FLOODING_STATIC_REWRITE is used
        BRN_WARN("Empty CS but neither NOREWRITE nor STATIC_REWRITE is used! Reject!");
        _cnt_reject_on_empty_cs++;
        output(1).push(p_in);
        return NULL;
    }
  }
  
  if ( rewrite ) {
    BRNPacketAnno::set_dst_ether_anno(p_in, next_hop);
    last_unicast_used = next_hop;
    BRN_DEBUG("* UnicastFlooding: Destination address rewrote to %s", next_hop.unparse().c_str());
    _cnt_rewrites++;
  } else {
    _cnt_bcasts++;
  }
    
  add_rewrite(&src, ntohs(bcast_header->bcast_id), &next_hop);

  return p_in;;
}

//-----------------------------------------------------------------------------
// Algorithms
//-----------------------------------------------------------------------------

bool
UnicastFlooding::algorithm_most_neighbours(EtherAddress &next_hop, Vector<EtherAddress> &neighbors, int hops)
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
    if (new_metric > best_new_metric) {
      best_new_metric = new_metric;
      best_nb = neighbors[x];
    }
        
    x_neighbours.clear();
    _fhelper->clear_graph(ng_list[x]);
  }


  local_neighbours.clear();
  _fhelper->clear_graph(local_graph);
  delete[] ng_list;
  
  BRN_DEBUG("Best: %s Metric: %d",best_nb.unparse().c_str(), best_new_metric);

  // check if we found a candidate and rewrite packet
  if (best_new_metric > -1) {
    next_hop = best_nb;
    return true;
  }

  return false;
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
      sa << "\" ucast_peer_metric=\"" << rewriter->get_ucast_peer_metric() << "\" reject_on_empty_cs=\"" << rewriter->get_reject_on_empty_cs();
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
