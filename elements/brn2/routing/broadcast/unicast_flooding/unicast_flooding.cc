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

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"

#include "unicast_flooding.hh"

CLICK_DECLS

UnicastFlooding::UnicastFlooding()
  :_me(),
   _link_table(),
   _cand_selection_strategy(0)
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

    switch (_cand_selection_strategy) {
      case 0: { // no rewriting; keep as broadcast
        output(0).push(p_in);
        return;
      }
      case 1: { // algo 1 - choose the neighbor with the largest neighborhood minus our own neighborhood

        // search in my 1-hop neighborhood
        Vector<EtherAddress> neighbors;

        if (_link_table) {
          const EtherAddress *me = _me->getMasterAddress();
          //_link_table->get_local_neighbors(neighbors);
          get_filtered_neighbors(*me, neighbors);

          if (neighbors.size() == 0) {
            BRN_DEBUG("* UnicastFlooding: We have only weak or no neighbors; keep broadcast!");
            output(0).push(p_in);
            return;
          }

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
				// rewrite address				
				memcpy(ether->ether_dhost, best_nb.data() , 6);
				BRN_DEBUG("* UnicastFlooding: Destination address rewrote to %s", best_nb.unparse().c_str());
				output(0).push(p_in);
				return;
			}
		}

		// no rewriting; keep as broadcast
		output(0).push(p_in);
		return;
	}
	case 2: { // algo 2 - choose the neighbor with the highest number of neighbors not covered by any other neighbor of the other neighbors

	 	// search in my 1-hop neighborhood
	 	Vector<EtherAddress> neighbors;
	
	       if (_link_table) {
			const EtherAddress *me = _me->getMasterAddress();
			get_filtered_neighbors(*me, neighbors);

			if (neighbors.size() == 0) {
				BRN_DEBUG("* UnicastFlooding: We have only weak or no neighbors; keep broadcast!");
				output(0).push(p_in);
				return;
			}	

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
					if (n_i == n_j) {
						continue;
					}

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
				// rewrite address				
				memcpy(ether->ether_dhost, best_nb.data() , 6);
				BRN_DEBUG("* UnicastFlooding: Destination address rewrote to %s", best_nb.unparse().c_str());
				output(0).push(p_in);
				return;
			}
		}

		// no rewriting; keep as broadcast
		output(0).push(p_in);
		return;

	}
  case 3: { // static rewriting
    memcpy(ether->ether_dhost, static_dst_mac.data() , 6);
    output(0).push(p_in);
    return;
  }
	default:
		BRN_WARN("* UnicastFlooding: Unknown candidate selection strategy; keep as broadcast.");
		output(0).push(p_in);
  }
 }

  /*
  if ( port == 1 ) { // received from other brn node; rewrite from unicast to broadcast
    // rewrite address
    memcpy(ether->ether_dhost, EtherAddress::make_broadcast().data() , 6);
    BRN_DEBUG("* UnicastFlooding: Destination address rewrote to bcast");
    output(1).push(p_in);
  }
  */

  return;
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
      // todo: show some rewrite statistics
      //sa << "<stats better_route=\"" << rreq->_stats_receive_better_route << "\"/>\n";
      //return ( sa.take_string() );
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
