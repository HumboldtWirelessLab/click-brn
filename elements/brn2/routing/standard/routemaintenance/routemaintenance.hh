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

#ifndef CLICK_ROUTINGMAINTENANCE_HH
#define CLICK_ROUTINGMAINTENANCE_HH

#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/standard/routingalgorithm.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn2/routing/standard/routingtable/brnroutingtable.hh"

#include "elements/brn2/brnelement.hh"

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
class RoutingMaintenance: public BRNElement {
 public:
  //
  //methods
  //
  /* generic click-mandated stuff*/
  RoutingMaintenance();
  ~RoutingMaintenance();

  void add_handlers();
  const char* class_name() const { return "RoutingMaintenance"; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void *cast(const char *n);

  //
  //member
  //
  void best_route(EtherAddress src, EtherAddress dst, Vector<EtherAddress> &route, uint32_t *metric);

  /**
   * @brief Query for a route between addrSrc and addrDst in cache
   * and link table.
   * @param addrSrc @a [in] The source of the route.
   * @param addrDst @a [in] The destination of the route.
   * @param route @a [in] Holds the route, if found.
   * @note Substitute for dijkstra and best_route
   */
  inline void query_route(
    /*[in]*/  const EtherAddress&   addrSrc,
    /*[in]*/  const EtherAddress&   addrDst,
    /*[out]*/ Vector<EtherAddress>& route,
              uint32_t *metric );

  inline void update_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route,
            uint32_t metric);

  Vector< Vector<EtherAddress> > top_n_routes(EtherAddress src, EtherAddress dst, int n);
  int32_t get_route_metric(const Vector<EtherAddress> &route);

  void best_route_to_me(EtherAddress src, Vector<EtherAddress> &route, uint32_t *metric);
  void best_route_from_me(EtherAddress dst, Vector<EtherAddress> &route, uint32_t *metric);
  uint32_t get_route_metric_to_me(EtherAddress s);
  uint32_t get_route_metric_from_me(EtherAddress d);

  bool valid_route(const Vector<EtherAddress> &route);

  String print_routes(bool);

private:

  BRN2NodeIdentity *_node_identity;
  Brn2LinkTable *_lt;
  BrnRoutingTable *_routing_table;
  RoutingAlgorithm *_routing_algo;

};

inline void
RoutingMaintenance::query_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route,
            uint32_t *metric )
{
  // First, search the route cache
  bool bCached = _routing_table->get_cached_route( addrSrc, addrDst, route, metric);

  if( ! bCached ) {
    _routing_algo->get_route(addrDst, addrSrc, route, metric);

    // Cache the found route ...
    if( ! route.empty() ) {
      _routing_table->insert_route( addrSrc, addrDst, route, *metric );
    }
  }
}

inline void
RoutingMaintenance::update_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route,
            uint32_t metric)
{

  Vector<EtherAddress> old_route;
  uint32_t old_metric;

  // First, search the route cache
  bool bCached = _routing_table->get_cached_route( addrSrc, addrDst, old_route, &old_metric );

  if( ! bCached ){
    _routing_table->insert_route( addrSrc, addrDst, route, metric );
  } else {
    if ( metric < old_metric ) {
      _routing_table->insert_route( addrSrc, addrDst, route, metric );
    }
  }
}

CLICK_ENDDECLS
#endif /* CLICK_ROUTINGMAINTENANCE_HH */
