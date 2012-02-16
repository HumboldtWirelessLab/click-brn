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
#include "elements/brn2/routing/standard/routingtable/brnroutingtable.hh"
#include "elements/brn2/routing/standard/routingalgorithm.hh"

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
class Dijkstra: public RoutingAlgorithm {
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

  //
  //member
  //

  Timestamp dijkstra_time;
  void dijkstra(EtherAddress src, bool);

  void calc_routes(EtherAddress src, bool from_me) { dijkstra(src, from_me); }
  const char *routing_algorithm_name() const { return "Dijkstra"; }

private:

  BRN2NodeIdentity *_node_identity;
  Brn2LinkTable *_lt;
  BrnRoutingTable *_brn_routetable;

  Timer _timer;

};

CLICK_ENDDECLS
#endif /* CLICK_BRNLINKTABLE_HH */
