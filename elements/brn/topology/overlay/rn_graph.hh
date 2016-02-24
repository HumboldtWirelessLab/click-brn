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

#ifndef RN_GRAPH_HH
#define RN_GRAPH_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <clicknet/ether.h>
#include <click/timer.hh>
#include <click/timestamp.hh>

#include "elements/brn/brnelement.hh"
#include <elements/brn/routing/identity/brn2_nodeidentity.hh>
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"

CLICK_DECLS

class RNGraph : public BRNElement {

 public:
  RNGraph();
  ~RNGraph();

  const char *class_name() const  { return "RNGraph"; }
  const char *port_count() const  { return "0/0"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  int initialize(ErrorHandler *);

  void add_handlers();

  void calc_neighbors();
  uint32_t metric2dist_sqr(uint32_t metric);

  static void static_calc_neighbors(Timer *, void *e) { (reinterpret_cast<RNGraph *>(e))->calc_neighbors(); }

  Timer _timer;

 private:

  BRN2NodeIdentity *_me;
  OverlayStructure *_ovl;
  Brn2LinkTable *_link_table;
  int _update_intervall;
  Vector<EtherAddress> _neighbors;
  uint32_t _threshold;

};

CLICK_ENDDECLS
#endif
