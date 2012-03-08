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
  : _node_identity(NULL),
    _routing_table(NULL),
    _routing_algo(NULL)
{
  BRNElement::init();
  stats_reset();
}

RoutingMaintenance::~RoutingMaintenance()
{
}

int
RoutingMaintenance::initialize (ErrorHandler *)
{
  return 0;
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
        "LINKTABLE", cpkP+cpkM, cpElement, &_lt,
        "ROUTETABLE", cpkP+cpkM, cpElement, &_routing_table,
        "ROUTINGALGORITHM", cpkP+cpkM, cpElement, &_routing_algo,
        "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd);

  return ret;
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

    if ( m >= _routing_algo->_min_link_metric_within_route ) {
      BRN_DEBUG(" * metric %d is inferior as min_metric %d", m, _routing_algo->_min_link_metric_within_route);
      return -1;
    }
  }
  return metric;
}

uint32_t
RoutingMaintenance::get_route_metric_to_me(EtherAddress s)
{
  return _routing_algo->metric_to_me(s);
}

uint32_t
RoutingMaintenance::get_route_metric_from_me(EtherAddress s)
{
  return _routing_algo->metric_from_me(s);
}

void
RoutingMaintenance::best_route(EtherAddress src, EtherAddress dst, Vector<EtherAddress> &route, uint32_t *metric)
{
  if (!dst) metric = 0;
  else {
    _routing_algo->get_route(src, dst, route, metric);
  }
}

void
RoutingMaintenance::best_route_to_me(EtherAddress src, Vector<EtherAddress> &route, uint32_t *metric)
{
  //TODO: route cache
  if (!src) metric = 0;
  else {
    _routing_algo->get_route(src, *_node_identity->getMasterAddress(), route, metric);
    if ( route.size() > 2 ) {
      if ( _node_identity->isIdentical(&route[route.size()-2]) ) route.pop_back();
    }
  }
}

void
RoutingMaintenance::best_route_from_me(EtherAddress dst, Vector<EtherAddress> &route, uint32_t *metric)
{
  //TODO: route cache
  if (!dst) metric = 0;
  else {
    _routing_algo->get_route(*_node_identity->getMasterAddress(), dst, route, metric);
    if ( route.size() > 1 ) {
      if ( _node_identity->isIdentical(&route[1]) ) route.pop_front();
    }
  }
}

bool
RoutingMaintenance::valid_route(const Vector<EtherAddress> &route)
{
  if (route.size() < 1) {
    return false;
  }
  /* ensure the metrics are all valid */
  unsigned metric = get_route_metric(route);
  if (metric  == 0 || metric >= BRN_LT_INVALID_ROUTE_METRIC) return false;

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

enum {H_ROUTES_FROM,
      H_ROUTES_TO,
      H_BEST_ROUTE,
      H_ALGO_AND_BEST_ROUTE,
      H_STATS};

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
    Vector <EtherAddress> r;
    if ( from_me )
      best_route_from_me(ether, r, &metric_trash);
    else
      best_route_to_me(ether, r, &metric_trash);

    if (valid_route(r)) {
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

String
RoutingMaintenance::print_stats()
{
  StringAccum sa;

  sa << "<routingmaintenancestats id=\"" << _node_identity->getMasterAddress()->unparse().c_str() << "\" >\n";
  sa << "\t<routes requests=\"" << _route_requests << "\" algo_usage=\"" << _route_algo_usages << "\" updates=\"" << _route_updates << "\" />\n";
  sa << "\t<cache hits=\"" << _cache_hits << "\" inserts=\"" << _cache_inserts << "\" updates=\"" << _cache_updates << "\" />\n</routingmaintenancestats>\n";

  return sa.take_string();

}


static String
RoutingMaintenance_read_param(Element *e, void *thunk)
{
  RoutingMaintenance *td = (RoutingMaintenance *)e;
    switch ((uintptr_t) thunk) {
    case H_ROUTES_TO: return td->print_routes(false);
    case H_ROUTES_FROM: return td->print_routes(true);
    case H_STATS: return td->print_stats();
    default:
      return String();
    }
}

static int
RoutingMaintenance_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  RoutingMaintenance *f = (RoutingMaintenance *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
    case H_BEST_ROUTE:
    case H_ALGO_AND_BEST_ROUTE: {
      EtherAddress dst;
      EtherAddress src;

      Vector<String> args;
      cp_spacevec(s, args);

      if (args.size() != 2)
        return errh->error("algo parameter must be etheraddress x etheraddress");
      if (!cp_ethernet_address(args[0], &src))
        return errh->error("algo parameter must be etheraddress x etheraddress");
      if (!cp_ethernet_address(args[1], &dst))
        return errh->error("algo parameter must be etheraddress x etheraddress");

      uint32_t metric_trash;
      Vector<EtherAddress> route;
      f->best_route(dst, src, route, &metric_trash);

      if ( route.size() > 1 ) {
        for (int j=0; j<route.size(); j++) {
          click_chatter(" - %d  %s", j, route[j].unparse().c_str());
        }
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

  add_read_handler("routes", RoutingMaintenance_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_from", RoutingMaintenance_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_to", RoutingMaintenance_read_param, (void *)H_ROUTES_TO);
  add_read_handler("stats", RoutingMaintenance_read_param, (void *)H_STATS);

  add_write_handler("algo_and_best_route", RoutingMaintenance_write_param, (void *)H_ALGO_AND_BEST_ROUTE);
  add_write_handler("best_route", RoutingMaintenance_write_param, (void *)H_BEST_ROUTE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RoutingMaintenance)
