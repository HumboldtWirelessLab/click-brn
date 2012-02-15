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
 * RouteMaintenance.{cc,hh}
 * R. Sombrutzki
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

#include "routemaintenance.hh"

CLICK_DECLS

RoutingMaintenance::RoutingMaintenance()
  : _node_identity(),
    _timer(this),
    _brn_dsr_min_link_metric_within_route(BRN_LT_DEFAULT_MIN_METRIC_IN_ROUTE)
{
  BRNElement::init();
}

RoutingMaintenance::~RoutingMaintenance()
{
}

int
RoutingMaintenance::initialize (ErrorHandler *)
{
  BRN2Device *dev;

  //_timer.initialize(this);
  //_timer.schedule_now();

  return 0;
}

void
RoutingMaintenance::run_timer(Timer*)
{
  //_timer.schedule_after_msec(5000);
}

void *
RoutingMaintenance::cast(const char *n)
{
  if (strcmp(n, "RoutingMaintenance") == 0)
    return (RoutingMaintenance *) this;
  else
    return 0;
}

int
RoutingMaintenance::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret = cp_va_kparse(conf, this, errh,
        "NODEIDENTITY", cpkP+cpkM, cpElement, &_node_identity,
        "ROUTETABLE", cpkP+cpkM, cpElement, &_brn_routetable,
        "ROUTINGALGORITHM", cpkP+cpkM, cpElement, &_routing_algo,
        "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd);

  return ret;
}

void
RoutingMaintenance::take_state(Element *e, ErrorHandler *) {
  RoutingMaintenance *q = (RoutingMaintenance *)e->cast("LinkTable");
  if (!q) return;
}

int32_t
RoutingMaintenance::get_route_metric(const Vector<EtherAddress> &route)
{

  if ( route.size() == 0 ) return -1;
  if ( route.size() == 1 ) return 0;

  unsigned metric = 0;
  for (int i = 0; i < route.size() - 1; i++) {
    EtherAddress src = route[i];
    EtherAddress dst = route[i+1];
    unsigned m = _lt->get_link_metric(src, dst);

    metric += m;

    if ( m >= _brn_dsr_min_link_metric_within_route ) {
      BRN_DEBUG(" * metric %d is inferior as min_metric %d", m, _brn_dsr_min_link_metric_within_route);
      return -1;
    }
  }
  return metric;
}


Vector<EtherAddress>
RoutingMaintenance::best_route(EtherAddress dst, bool from_me, uint32_t *metric)
{
  Vector<EtherAddress> reverse_route;

  if (!dst) {
    metric = 0;
    return reverse_route;
  }
  BrnHostInfo *nfo = _lt->_hosts.findp(dst);

  if (from_me) {
    while (nfo && nfo->_metric_from_me != 0) {
      reverse_route.push_back(nfo->_ether);
      *metric = nfo->_metric_from_me;
      nfo = _lt->_hosts.findp(nfo->_prev_from_me);
    }
    if (nfo && nfo->_metric_from_me == 0) {
      reverse_route.push_back(nfo->_ether);
    }
  } else {
    while (nfo && nfo->_metric_to_me != 0) {
      reverse_route.push_back(nfo->_ether);
      *metric = nfo->_metric_to_me;
      nfo = _lt->_hosts.findp(nfo->_prev_to_me);
    }
    if (nfo && nfo->_metric_to_me == 0) {
      reverse_route.push_back(nfo->_ether);
    }
  }

  return reverse_route;
}


String
RoutingMaintenance::print_routes(bool from_me)
{
  StringAccum sa;

  Vector<EtherAddress> ether_addrs;

  for (HTIter iter = _lt->_hosts.begin(); iter.live(); iter++)
    ether_addrs.push_back(iter.key());

  click_qsort(ether_addrs.begin(), ether_addrs.size(), sizeof(EtherAddress), etheraddr_sorter);

  sa << "<routetable id=\"";
  sa << _node_identity->getMasterAddress()->unparse().c_str();
  sa << "\">\n";

  for (int x = 0; x < ether_addrs.size(); x++) {
    EtherAddress ether = ether_addrs[x];
    uint32_t metric_trash;
    Vector <EtherAddress> r = best_route(ether, from_me, &metric_trash);
    if (_lt->valid_route(r)) {
      sa << "\t<route from=\"" << r[0] << "\" to=\"" << r[r.size()-1] << "\">\n";

      for (int i = 0; i < r.size()-1; i++) {
        EthernetPair pair = EthernetPair(r[i], r[i+1]);
        BrnLinkInfo *l = _lt->_links.findp(pair);
        sa << "\t\t<link from=\"" << r[i] << "\" to=\"" << r[i+1] << "\" ";
        sa << "metric=\"" << l->_metric << "\" ";
        sa << "seq=\"" << l->_seq << "\" age=\"" << l->age() << "\" />\n";
      }
      sa << "\t</route>\n";

    }
  }

  sa << "</routetable>\n";

  return sa.take_string();
}

bool
RoutingMaintenance::valid_route(const Vector<EtherAddress> &route)
{
  if (route.size() < 1) {
    return false;
  }
  /* ensure the metrics are all valid */
  unsigned metric = get_route_metric(route);
  if (metric  == 0 ||
      metric >= BRN_LT_INVALID_ROUTE_METRIC){
    return false;
      }

      /* ensure that a node appears no more than once */
      for (int x = 0; x < route.size(); x++) {
        for (int y = x + 1; y < route.size(); y++) {
          if (route[x] == route[y]) {
            return false;
          }
        }
      }

      return true;
}

void
RoutingMaintenance::dijkstra(EtherAddress src, bool from_me)
{

  if (!_lt->_hosts.findp(src)) {
    return;
  }

  Timestamp start = Timestamp::now();

  typedef HashMap<EtherAddress, bool> EtherMap;
  EtherMap ether_addrs;

  for (HTIter iter = _lt->_hosts.begin(); iter.live(); iter++) {
    ether_addrs.insert(iter.value()._ether, true);
  }

  for (EtherMap::const_iterator i = ether_addrs.begin(); i.live(); i++) {
    /* clear them all initially */
    BrnHostInfo *n = _lt->_hosts.findp(i.key());
    assert(n);
    n->clear(from_me);
  }
  BrnHostInfo *root_info = _lt->_hosts.findp(src);

  assert(root_info);

  if (from_me) {
    root_info->_prev_from_me = root_info->_ether;
    root_info->_metric_from_me = 0;
  } else {
    root_info->_prev_to_me = root_info->_ether;
    root_info->_metric_to_me = 0;
  }

  EtherAddress current_min_ether = root_info->_ether;

  while (current_min_ether) {
    BrnHostInfo *current_min = _lt->_hosts.findp(current_min_ether);
    assert(current_min);
    if (from_me) {
      current_min->_marked_from_me = true;
    } else {
      current_min->_marked_to_me = true;
    }

    for (EtherMap::const_iterator i = ether_addrs.begin(); i.live(); i++) {
      BrnHostInfo *neighbor = _lt->_hosts.findp(i.key());
      assert(neighbor);
      bool marked = neighbor->_marked_to_me;
      if (from_me) {
        marked = neighbor->_marked_from_me;
      }

      if (marked) {
        continue;
      }

      EthernetPair pair = EthernetPair(neighbor->_ether, current_min_ether);
      if (from_me) {
        pair = EthernetPair(current_min_ether, neighbor->_ether);
      }
      BrnLinkInfo *lnfo = _lt->_links.findp(pair);
      if (NULL == lnfo || 0 == lnfo->_metric 
          || BRN_LT_INVALID_LINK_METRIC <= lnfo->_metric) {
        continue;
      }
      uint32_t neighbor_metric = neighbor->_metric_to_me;
      uint32_t current_metric = current_min->_metric_to_me;

      if (from_me) {
        neighbor_metric = neighbor->_metric_from_me;
        current_metric = current_min->_metric_from_me;
      }

      uint32_t adjusted_metric = current_metric + lnfo->_metric;
      if (!neighbor_metric || 
          adjusted_metric < neighbor_metric) {
        if (from_me) {
          neighbor->_metric_from_me = adjusted_metric;
          neighbor->_prev_from_me = current_min_ether;
        } else {
          neighbor->_metric_to_me = adjusted_metric;
          neighbor->_prev_to_me = current_min_ether;
        }
      }
    }

    current_min_ether = EtherAddress();
    uint32_t  min_metric = ~0;
    for (EtherMap::const_iterator i = ether_addrs.begin(); i.live(); i++) {
      BrnHostInfo *nfo = _lt->_hosts.findp(i.key());
      assert(nfo);
      uint32_t metric = nfo->_metric_to_me;
      bool marked = nfo->_marked_to_me;
      if (from_me) {
        metric = nfo->_metric_from_me;
        marked = nfo->_marked_from_me;
      }
      if (!marked && metric && 
        metric < min_metric) {
        current_min_ether = nfo->_ether;
        min_metric = metric;
      }
    }
  }

  dijkstra_time = Timestamp::now()-start;

  //if BRNDEBUG...
  //StringAccum sa;
  //sa << "dijstra took " << finish - start;
  //click_chatter("%s: %s\n", id().c_str(), sa.take_string().c_str());
}


enum {H_ROUTES_FROM,
      H_ROUTES_TO,
      H_DIJKSTRA,
      H_DIJKSTRA_TIME,
      H_BEST_ROUTE,
      H_BESTROUTE_DIJKSTRA};

static String 
LinkTable_read_param(Element *e, void *thunk)
{
  RoutingMaintenance *td = (RoutingMaintenance *)e;
    switch ((uintptr_t) thunk) {
    case H_DIJKSTRA_TIME: {
      StringAccum sa;
      sa << td->dijkstra_time << "\n";
      return sa.take_string();
    }
    case H_ROUTES_TO: return td->print_routes(false);
    case H_ROUTES_FROM: return td->print_routes(true);
    default:
      return String();
    }
}

static int 
LinkTable_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  RoutingMaintenance *f = (RoutingMaintenance *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
  case H_DIJKSTRA: {
    // run dijkstra for all associated clients
    EtherAddress m;
    if (!cp_ethernet_address(s, &m)) 
      return errh->error("dijkstra parameter must be etheraddress");

    f->dijkstra(m, true);
    break;
  }
  case H_BEST_ROUTE: {
    EtherAddress m;
    if (!cp_ethernet_address(s, &m)) 
      return errh->error("dijkstra parameter must be etheraddress");

    uint32_t metric_trash;
    Vector<EtherAddress> route = f->best_route(m, true, &metric_trash);

    for (int j=0; j<route.size(); j++) {
      click_chatter(" - %d  %s", j, route[j].unparse().c_str());
    }
    break;
  }
  case H_BESTROUTE_DIJKSTRA: {
    Vector<String> args;
    cp_spacevec(s, args);

    EtherAddress src, dst;
    if (args.size() != 2)
      return errh->error("dijkstra parameter must be etheraddress x etheraddress");
    if (!cp_ethernet_address(args[0], &src)) 
      return errh->error("dijkstra parameter must be etheraddress x etheraddress");
    if (!cp_ethernet_address(args[1], &dst)) 
      return errh->error("dijkstra parameter must be etheraddress x etheraddress");

    f->dijkstra(src, true);
    uint32_t metric_trash;
    Vector<EtherAddress> route = f->best_route(dst, true, &metric_trash);

    for (int j=0; j<route.size(); j++) {
      click_chatter(" - %d  %s", j, route[j].unparse().c_str());
    }
    break;
  }
  }
  return 0;
}

void
RoutingMaintenance::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("routes", LinkTable_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_from", LinkTable_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_to", LinkTable_read_param, (void *)H_ROUTES_TO);
  add_read_handler("dijkstra_time", LinkTable_read_param, (void *)H_DIJKSTRA_TIME);

  add_write_handler("dijkstra", LinkTable_write_param, (void *)H_DIJKSTRA);
  add_write_handler("best_route", LinkTable_write_param, (void *)H_BEST_ROUTE);
  add_write_handler("best_route_and_dijkstra", LinkTable_write_param, (void *)H_BESTROUTE_DIJKSTRA);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Dijkstra)
