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

#ifndef CLICK_BRNDIJKSTRA_HH
#define CLICK_BRNDIJKSTRA_HH

#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/routing/standard/routingtable/brnroutingtable.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/routing/standard/routingalgorithm.hh"

#include "elements/brn/brnelement.hh"

CLICK_DECLS
/*
 * =c
 * BrnLinkTable(Ethernet Address, [STALE timeout])
 * =s BRN
 * Keeps a Link state database and calculates Weighted Shortest Path 
 * for other elements
 * =d
 * Runs dijkstra's algorithm occasionally.
 *
 */

/*
 * Represents a link table storing {@link BrnLink} links.
 */

class Dijkstra: public RoutingAlgorithm {

#define DIJKSTRA_MAX_GRAPHS 16
#define DIJKSTRA_GRAPH_MODE_UNUSED    0
#define DIJKSTRA_GRAPH_MODE_FR0M_NODE 1
#define DIJKSTRA_GRAPH_MODE_TO_NODE   2


  class DijkstraGraphInfo {
   public:
    uint8_t _mode;
    EtherAddress _node;
    Timestamp _last_used;
    uint32_t _no_calcs;

    DijkstraGraphInfo(): _mode(DIJKSTRA_GRAPH_MODE_UNUSED), _node(EtherAddress()), _no_calcs(0) {
      _last_used = Timestamp::now();
    }

    DijkstraGraphInfo(uint8_t mode,EtherAddress node): _mode(mode), _node(node), _last_used(Timestamp::now()), _no_calcs(0) {}

    inline bool equals(DijkstraGraphInfo dgi) {
      return ((dgi._mode == _mode) && ( dgi._node == _node ));
    }
  };

  class DijkstraNodeInfo {
    public:
      EtherAddress _ether;

      uint32_t _metric[DIJKSTRA_MAX_GRAPHS];
      DijkstraNodeInfo *_next[DIJKSTRA_MAX_GRAPHS];
      bool _marked[DIJKSTRA_MAX_GRAPHS];

      DijkstraNodeInfo(EtherAddress p) {
        _ether = p;
        memset(_metric, 0, DIJKSTRA_MAX_GRAPHS * sizeof(uint32_t));
        memset(_next, 0, DIJKSTRA_MAX_GRAPHS * sizeof(DijkstraNodeInfo *));
        memset(_marked, 0, DIJKSTRA_MAX_GRAPHS * sizeof(bool));
      }

      void clear(uint32_t index) {
        _next[index] = NULL;
        _metric[index] = 0;
        _marked[index] = false;
      }
  };

  typedef HashMap<EtherAddress, DijkstraNodeInfo*> DNITable;
  typedef DNITable::const_iterator DNITIter;

 public:
  //
  //methods
  //
  /* generic click-mandated stuff*/
  Dijkstra();
  ~Dijkstra();

  void add_handlers();
  const char* class_name() const { return "Dijkstra"; }

  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void take_state(Element *, ErrorHandler *);
  void *cast(const char *n);

  //
  //member
  //
  const char *routing_algorithm_name() const { return "Dijkstra"; }

  Timestamp dijkstra_time;
  int dijkstra(EtherAddress node, uint8_t mode);

  void get_route(EtherAddress src, EtherAddress dst, Vector<EtherAddress> &route, uint32_t *metric);
  int32_t metric_from_me(EtherAddress dst);
  int32_t metric_to_me(EtherAddress src);

  void add_node(BrnHostInfo *);
  void remove_node(BrnHostInfo *);

  void update_link(BrnLinkInfo *link);

  String stats();

 private:

  void dijkstra(int graph_index);
  int get_graph_index(EtherAddress ea, uint8_t mode);

  BRN2NodeIdentity *_node_identity;
  Brn2LinkTable *_lt;

  DNITable _dni_table;
  
  DijkstraNodeInfo **_dni_array;
  uint32_t _dni_array_max_size;
  
  DijkstraGraphInfo _dgi_list[DIJKSTRA_MAX_GRAPHS];

  uint32_t _max_graph_age;

};

CLICK_ENDDECLS
#endif /* CLICK_BRNLINKTABLE_HH */
