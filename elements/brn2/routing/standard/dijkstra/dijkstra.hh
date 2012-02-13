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
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/routecache/brn2routecache.hh"

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
class Dijkstra: public BRNElement {
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
  void run_timer(Timer*);
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void take_state(Element *, ErrorHandler *);
  void *cast(const char *n);

  Vector< Vector<EtherAddress> > top_n_routes(EtherAddress dst, int n);

  //
  //member
  //

  Timestamp dijkstra_time;
  void dijkstra(EtherAddress src, bool);
  Vector<EtherAddress> best_route(EtherAddress dst, bool from_me, uint32_t *metric);
  bool valid_route(const Vector<EtherAddress> &route);

  int32_t get_route_metric(const Vector<EtherAddress> &route);
  void get_inodes(Vector<EtherAddress> &ether_addrs);

  String print_routes(bool);

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
    /*[out]*/ Vector<EtherAddress>& route );

  inline void update_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route,
  uint32_t metric);

private:

  BRN2NodeIdentity *_node_identity;
  Brn2LinkTable *_lt;
  Brn2RouteCache *_brn_routetable;

  Timer _timer;

  uint32_t _brn_dsr_min_link_metric_within_route;

};

inline void
Dijkstra::query_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route )
{
  uint32_t metric;

  // First, search the route cache
  bool bCached = _brn_routetable->get_cached_route( addrSrc,
                                                    addrDst,
                                                    route,
                                                    &metric);

  if( false == bCached )
  {
    // current node is not final destination of the packet,
    // so lookup route from dsr table and generate a dsr packet
    dijkstra(addrSrc, true);

    route = best_route(addrDst, true, &metric);

    // Cache the found route ...
    if( false == route.empty() )
    {
      _brn_routetable->insert_route( addrSrc, addrDst, route, metric );
    }
  }
}

inline void
Dijkstra::update_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route,
            uint32_t metric)
{

  Vector<EtherAddress> old_route;
  uint32_t old_metric;

  // First, search the route cache
  bool bCached = _brn_routetable->get_cached_route( addrSrc, addrDst, old_route, &old_metric );

  if( ! bCached )
  {
    // current node is not final destination of the packet,
    // so lookup route from dsr table and generate a dsr packet
    _brn_routetable->insert_route( addrSrc, addrDst, route, metric );
  } else {
    if ( metric < old_metric ) {
      _brn_routetable->insert_route( addrSrc, addrDst, route, metric );
    }
  }
}

CLICK_ENDDECLS
#endif /* CLICK_BRNLINKTABLE_HH */
