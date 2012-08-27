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
 * Dijkstra.{cc,hh} -- Shortes Path
 * R. Sombrutzki (A. Zubow)
 *
 */
#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"

#include "dijkstra.hh"

CLICK_DECLS

static const char* dijkstra_graph_mode_strings[] = { "Unused", "FromNode", "ToNode" };

Dijkstra::Dijkstra()
  : _node_identity(),
    _max_graph_age(0)
{
  RoutingAlgorithm::init();
  _min_link_metric_within_route = BRN_LT_DEFAULT_MIN_METRIC_IN_ROUTE;
}

Dijkstra::~Dijkstra()
{
}

void *
Dijkstra::cast(const char *n)
{
  if (strcmp(n, "Dijkstra") == 0)
    return (Dijkstra *) this;
  else
    return 0;
}

int
Dijkstra::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret = cp_va_kparse(conf, this, errh,
        "NODEIDENTITY", cpkP+cpkM, cpElement, &_node_identity,
        "LINKTABLE", cpkP+cpkM, cpElement, &_lt,
        "ROUTETABLE", cpkP+cpkM, cpElement, &_brn_routetable,
        "MIN_LINK_METRIC_IN_ROUTE", cpkP+cpkM, cpInteger, &_min_link_metric_within_route,
        "MAXGRAPHAGE", cpkP, cpInteger, &_max_graph_age,
        "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd);

  return ret;
}

int
Dijkstra::initialize (ErrorHandler *)
{
  _lt->add_informant((BrnLinkTableChangeInformant*)this);

  _dgi_list[0]._node = *_node_identity->getMasterAddress();
  _dgi_list[0]._mode = DIJKSTRA_GRAPH_MODE_FR0M_NODE;
  _dgi_list[1]._node = *_node_identity->getMasterAddress();
  _dgi_list[1]._mode = DIJKSTRA_GRAPH_MODE_TO_NODE;

  return 0;
}

void
Dijkstra::take_state(Element *e, ErrorHandler *) {
  Dijkstra *q = (Dijkstra *)e->cast("LinkTable");
  if (!q) return;
}

void
Dijkstra::get_route(EtherAddress src, EtherAddress dst, Vector<EtherAddress> &route, uint32_t * /*metric*/)
{
  //metric = 0;

  if (( _dni_table.find(dst) == NULL ) || ( _dni_table.find(src) == NULL ) ) return;

  int graph_id_src = -1;
  int graph_id_dst = -1;
  int graph_id_me = -1;

  if ( _node_identity->isIdentical(&src) && (_dgi_list[0]._mode == DIJKSTRA_GRAPH_MODE_FR0M_NODE) ) graph_id_me = 0;
  else if ( _node_identity->isIdentical(&dst) && (_dgi_list[1]._mode == DIJKSTRA_GRAPH_MODE_TO_NODE) ) graph_id_me = 1;

  for ( int i = 2; i < DIJKSTRA_MAX_GRAPHS; i++ ) {
    if ( (_dgi_list[i]._node == src) && (_dgi_list[i]._mode == DIJKSTRA_GRAPH_MODE_FR0M_NODE) ) graph_id_src = i;
    else if ( (_dgi_list[i]._node == dst) && (_dgi_list[i]._mode == DIJKSTRA_GRAPH_MODE_TO_NODE) ) graph_id_dst = i;
  }

  int graph_id = -1;

  if ( (graph_id_src == -1) && (graph_id_dst == -1) && (graph_id_me == -1) ) {
    if ( _node_identity->isIdentical(&src) ) {                        // try to calc routes
      graph_id = get_graph_index(src, DIJKSTRA_GRAPH_MODE_FR0M_NODE); // from me
    } else if ( _node_identity->isIdentical(&dst) ) {                 // or
      graph_id = get_graph_index(dst, DIJKSTRA_GRAPH_MODE_TO_NODE);   // to me (prefere this)
    } else {
      graph_id = get_graph_index(src, DIJKSTRA_GRAPH_MODE_FR0M_NODE);
    }

    dijkstra(graph_id);
  } else { //if you found graph, choose the latest one
    if (graph_id_me != -1) graph_id = graph_id_me;
    if ((graph_id_src != -1) && ((graph_id == -1) || (_dgi_list[graph_id_src]._last_used > _dgi_list[graph_id]._last_used)))
      graph_id = graph_id_src;
    if ((graph_id_dst != -1) && ((graph_id == -1) || (_dgi_list[graph_id_dst]._last_used > _dgi_list[graph_id]._last_used)))
      graph_id = graph_id_dst;

    if ( (_dgi_list[graph_id]._last_used == Timestamp(0,0)) || // if also the latest not ready (graphs with links from or to me)
         ((Timestamp::now() - _dgi_list[graph_id]._last_used).msecval() > _max_graph_age) ) { // with links from or to me) or
                                                                                 // or if the graph is too old -> start dijkstra
      dijkstra(graph_id);
    }
  }

  BRN_DEBUG("Graph ID: %d",graph_id);

  EtherAddress s_ea = _node_identity->isIdentical(&src)?*_node_identity->getMasterAddress():src;
  EtherAddress d_ea = _node_identity->isIdentical(&dst)?*_node_identity->getMasterAddress():dst;

  DijkstraNodeInfo *s_node = _dni_table.find(s_ea);
  DijkstraNodeInfo *d_node = _dni_table.find(d_ea);

  route.clear();

  if ( ! s_node->_marked[graph_id] || ! d_node->_marked[graph_id] ) return;

  if ( _dgi_list[graph_id]._mode == DIJKSTRA_GRAPH_MODE_FR0M_NODE ) {
    DijkstraNodeInfo *current_node = _dni_table.find(d_ea);
    while ( current_node->_ether != s_ea ) {
      BRN_DEBUG("Next add to route(from): %s",current_node->_ether.unparse().c_str());
      route.push_front(current_node->_ether);
      current_node = current_node->_next[graph_id];
    }
    route.push_front(current_node->_ether);
  } else {
    DijkstraNodeInfo *current_node = _dni_table.find(s_ea);
    if(!current_node) {
    	BRN_ERROR("Null pointer exception in Dijkstra::get_route.");
    	return;
    }

    while ( current_node->_ether != d_ea ) {
      BRN_DEBUG("Next add to route(to): %s",current_node->_ether.unparse().c_str());
      route.push_back(current_node->_ether);
      current_node = current_node->_next[graph_id];
      if (!current_node || !current_node->_ether) {
    	  BRN_ERROR("Null pointer exception in Dijkstra::get_route");
    	  return;
      }
    }
    route.push_back(current_node->_ether);
  }

  // TODO: fix dst and src is it s me and if we have multiple radios/devices
}

int32_t
Dijkstra::metric_from_me(EtherAddress dst)
{
  DijkstraNodeInfo *dni = _dni_table.find(dst);

  if ( ! dni ) return -1;

  if ( (_dgi_list[0]._last_used == Timestamp(0,0)) ||
       ((Timestamp::now() - _dgi_list[0]._last_used).msecval() > _max_graph_age) ) dijkstra(0);

  return dni->_metric[0];
}

int32_t
Dijkstra::metric_to_me(EtherAddress src)
{
  DijkstraNodeInfo *dni = _dni_table.find(src);

  if ( ! dni ) return -1;

  if ( (_dgi_list[1]._last_used == Timestamp(0,0)) ||
        ((Timestamp::now() - _dgi_list[1]._last_used).msecval() > _max_graph_age) ) dijkstra(1);

  return dni->_metric[1];
}

int
Dijkstra::dijkstra(EtherAddress node, uint8_t mode)
{
  int index = get_graph_index(node, mode);

  if ( (index >= 0) && (index < DIJKSTRA_MAX_GRAPHS) ) dijkstra(index);

  return index;
}

void
Dijkstra::dijkstra(int graph_index)
{

  DijkstraGraphInfo *dgi = &(_dgi_list[graph_index]);
  dgi->_no_calcs++;

  if ( BRN_DEBUG_LEVEL_DEBUG ) {
    BRN_LOG(BrnLogger::LOG,"Graph index: %d",graph_index);
    BRN_LOG(BrnLogger::LOG,"Node: %s Mode: %s Last used: %s",dgi->_node.unparse().c_str(), dijkstra_graph_mode_strings[dgi->_mode],
                                                  dgi->_last_used.unparse().c_str());
  }

  if (!_dni_table.find(dgi->_node)) {
    BRN_WARN("Couldn't find node");
    return;
  }

  Timestamp start = Timestamp::now();

  for (DNITIter i = _dni_table.begin(); i.live(); i++) {
    /* clear them all initially */
    DijkstraNodeInfo *dni = i.value();
    dni->clear(graph_index);
  }

  DijkstraNodeInfo *root_info = _dni_table.find(dgi->_node);

  assert(root_info);

  root_info->_next[graph_index] = root_info;
  root_info->_metric[graph_index] = 0;

  EtherAddress current_min_ether = root_info->_ether;

  while (current_min_ether) {
    DijkstraNodeInfo *current_min = _dni_table.find(current_min_ether);
    assert(current_min);

    current_min->_marked[graph_index] = true;

    for (DNITIter i = _dni_table.begin(); i.live(); i++) {
      DijkstraNodeInfo *neighbor = i.value();

      assert(neighbor);

      if (neighbor->_marked[graph_index]) continue;

      EthernetPair pair = EthernetPair(neighbor->_ether, current_min_ether);

      if (_dgi_list[graph_index]._mode == DIJKSTRA_GRAPH_MODE_FR0M_NODE) {
        pair = EthernetPair(current_min_ether, neighbor->_ether);
      }

      BrnLinkInfo *lnfo = _lt->_links.findp(pair);
      if (NULL == lnfo || 0 == lnfo->_metric || BRN_LT_INVALID_LINK_METRIC <= lnfo->_metric) {
        continue;
      }

      uint32_t neighbor_metric = neighbor->_metric[graph_index];
      uint32_t current_metric = current_min->_metric[graph_index];

      uint32_t adjusted_metric = current_metric + lnfo->_metric;

      if (!neighbor_metric || adjusted_metric < neighbor_metric) {
        neighbor->_metric[graph_index] = adjusted_metric;
        neighbor->_next[graph_index] = _dni_table.find(current_min_ether);
      }
    }

    current_min_ether = EtherAddress();
    uint32_t  min_metric = ~0;

    for (DNITIter i = _dni_table.begin(); i.live(); i++) {
      DijkstraNodeInfo *dni = i.value();
      assert(dni);

      uint32_t metric = dni->_metric[graph_index];
      bool marked = dni->_marked[graph_index];

      if (!marked && metric && metric < min_metric) {
        current_min_ether = dni->_ether;
        min_metric = metric;
      }
    }
  }

  dgi->_last_used = Timestamp::now();

  dijkstra_time = dgi->_last_used - start;

  BRN_DEBUG("dijstra took %s",dijkstra_time.unparse().c_str());
}

int
Dijkstra::get_graph_index(EtherAddress ea, uint8_t mode)
{
  if (!_dni_table.find(ea)) {
    BRN_WARN("Couldn't find node");
    return -1;
  }

  if ( _node_identity->isIdentical(&ea) ) {
    if ( mode == DIJKSTRA_GRAPH_MODE_FR0M_NODE ) return 0;
    if ( mode == DIJKSTRA_GRAPH_MODE_TO_NODE ) return 1;
    return -1;
  }

  int unused = -1;

  for ( int i = 2; i < DIJKSTRA_MAX_GRAPHS; i++ ) {
    if ( (_dgi_list[i]._mode == mode) && (_dgi_list[i]._node == ea) ) return i;
    if ( (_dgi_list[i]._mode == DIJKSTRA_GRAPH_MODE_UNUSED) && (unused == -1) ) {
      unused = i;
      break;
    }
  };

  if ( unused == -1 ) {
    Timestamp now = Timestamp::now();
    int last_used = -1;
    int oldest = -1;

    for ( int i = 2; i < DIJKSTRA_MAX_GRAPHS; i++ ) {
      if (oldest == -1) {
        oldest = i;
        last_used = (now - _dgi_list[i]._last_used).msecval();
      } else {
        int diff = (now - _dgi_list[i]._last_used).msecval();
        if ( diff > last_used ) {
          oldest = i;
          last_used = diff;
        }
      }
    }

    unused = oldest;
  }

  _dgi_list[unused]._node = ea;
  _dgi_list[unused]._mode = mode;
  _dgi_list[unused]._no_calcs = 0;

  return unused;
}

void
Dijkstra::add_node(BrnHostInfo *bhi)
{
  _dni_table.insert(bhi->_ether, new DijkstraNodeInfo(bhi->_ether, bhi->_ip));
}

void
Dijkstra::remove_node(BrnHostInfo *bhi)
{
  DijkstraNodeInfo *info = _dni_table.find(bhi->_ether);
  if ( info != NULL ) {
    delete info;
    _dni_table.erase(bhi->_ether);
  }
}

/****************************************************************************************************************/
/*********************************** H A N D L E R **************************************************************/
/****************************************************************************************************************/

String
Dijkstra::stats()
{
  StringAccum sa;
  int c = 0;
  for ( int i = 0; i < DIJKSTRA_MAX_GRAPHS; i++ ) if (_dgi_list[i]._mode != DIJKSTRA_GRAPH_MODE_UNUSED) c++; 

  sa << "<dijkstra node=\"" << BRN_NODE_NAME << "\" used_graphs=\"" << c << "\" max_graphs=\"" << DIJKSTRA_MAX_GRAPHS;
  sa << "\" max_age=\"" << _max_graph_age << "\" >\n";

  for ( int i = 0; i < DIJKSTRA_MAX_GRAPHS; i++ ) {
    sa << "\t<graph id=\"" << i << "\" node=\"" << _dgi_list[i]._node.unparse() << "\" mode=\"";
    sa << dijkstra_graph_mode_strings[_dgi_list[i]._mode] << "\" last_used=\"" << _dgi_list[i]._last_used.unparse();
    sa << "\" no_calcs=\"" << _dgi_list[i]._no_calcs << "\" />\n";
  }

  sa << "</dijkstra>\n";

  return sa.take_string();
}

enum { H_DIJKSTRA,
       H_DIJKSTRA_TO,
       H_DIJKSTRA_FROM,
       H_DIJKSTRA_ROUTE,
       H_DIJKSTRA_TIME,
       H_DIJKSTRA_STATS};

static String
Dijkstra_read_param(Element *e, void *thunk)
{
  Dijkstra *td = (Dijkstra *)e;
    switch ((uintptr_t) thunk) {
    case H_DIJKSTRA_TIME: {
      StringAccum sa;
      sa << td->dijkstra_time << "\n";
      return sa.take_string();
    }
    case H_DIJKSTRA_STATS: {
      return td->stats();
    }
    default:
      return String();
    }
}

static int
Dijkstra_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  Dijkstra *f = (Dijkstra *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
    case H_DIJKSTRA_TO:
    case H_DIJKSTRA_FROM: {
      // run dijkstra for all associated clients
      EtherAddress m;
      if (!cp_ethernet_address(s, &m)) return errh->error("dijkstra parameter must be etheraddress");

      f->dijkstra(m, ((long)vparam == H_DIJKSTRA_TO)?DIJKSTRA_GRAPH_MODE_TO_NODE:DIJKSTRA_GRAPH_MODE_FR0M_NODE);
      break;
    }
    case H_DIJKSTRA: {
      // run dijkstra for all associated clients
        EtherAddress m;
        if (!cp_ethernet_address(s, &m)) return errh->error("dijkstra parameter must be etheraddress");

        f->dijkstra(m,DIJKSTRA_GRAPH_MODE_TO_NODE);
        f->dijkstra(m,DIJKSTRA_GRAPH_MODE_FR0M_NODE);
        break;
      }
      case H_DIJKSTRA_ROUTE: {
        Vector<String> args;
        cp_spacevec(s, args);
      // run dijkstra for all associated clients
        EtherAddress s,d;

        if (!cp_ethernet_address(args[0], &s)) return errh->error("dijkstra parameter must be etheraddress");
        if (!cp_ethernet_address(args[1], &d)) return errh->error("dijkstra parameter must be etheraddress");

        uint32_t metric;
        Vector<EtherAddress> r;
        f->get_route(s, d, r, &metric);

        for ( int i = 0; i < r.size(); i++ ) {
          click_chatter("%d -> %s",i,r[i].unparse().c_str());
        }
        break;
      }
  }
  return 0;
}

void
Dijkstra::add_handlers()
{
  RoutingAlgorithm::add_handlers();

  add_write_handler("dijkstra", Dijkstra_write_param, (void *)H_DIJKSTRA);
  add_write_handler("dijkstra_from_node", Dijkstra_write_param, (void *)H_DIJKSTRA_FROM);
  add_write_handler("dijkstra_to_node", Dijkstra_write_param, (void *)H_DIJKSTRA_TO);
  add_write_handler("dijkstra_route", Dijkstra_write_param, (void *)H_DIJKSTRA_ROUTE);

  add_read_handler("dijkstra_time", Dijkstra_read_param, (void *)H_DIJKSTRA_TIME);
  add_read_handler("stats", Dijkstra_read_param, (void *)H_DIJKSTRA_STATS);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(Dijkstra)
