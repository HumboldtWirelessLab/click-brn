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

#ifndef FLOODING_HELPER_ELEMENT_HH
#define FLOODING_HELPER_ELEMENT_HH

#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"


CLICK_DECLS

/*
 * =c
 * FloodingHelper()
 * =s converts a broadcast packet into a unicast packet
 * output: rewrited packet
 * =d
 *
 */

class NeighbourMetric {
 public:
  EtherAddress _ea;
  uint16_t     _metric;
  uint8_t      _flags;

  NeighbourMetric(EtherAddress ea, uint16_t metric ) {
    _ea = ea;
    _metric = metric;
    _flags = 0;
  }
};

typedef Vector<NeighbourMetric> NeighbourMetricList;
typedef NeighbourMetricList::const_iterator NeighbourMetricListIter;

typedef HashMap<EtherAddress, NeighbourMetric*> NeighbourMetricMap;
typedef NeighbourMetricMap::const_iterator NeighbourMetricMapIter;

class FloodingHelper : public BRNElement {

 public:
  
  //
  //methods
  //
  FloodingHelper();
  ~FloodingHelper();

  const char *class_name() const  { return "FloodingHelper"; }
  const char *port_count() const  { return "0/0"; }
  const char *processing() const { return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

public: 
  //
  //member
  //
  Brn2LinkTable *_link_table;

 public:
  int _max_metric_to_neighbor; // max. metric towards a neighbor

  void static print_vector(Vector<EtherAddress> &eas);
  uint32_t static metric2pdr(uint32_t metric);

  // helper
  void get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out);
  int subtract_and_cnt(const Vector<EtherAddress> &s1, const Vector<EtherAddress> &s2);
  void addAll(const Vector<EtherAddress> &newS, Vector<EtherAddress> &inout);

  int findWorst(const EtherAddress &src, Vector<EtherAddress> &neighbors);
  void filter_bad_one_hop_neighbors(const EtherAddress &node, Vector<EtherAddress> &neighbors, Vector<EtherAddress> &filtered_neighbors);
  void filter_bad_one_hop_neighbors_with_all_known_nodes(const EtherAddress &node, Vector<EtherAddress> &neighbors, Vector<EtherAddress> &filtered_neighbors);
  void filter_known_one_hop_neighbors(Vector<EtherAddress> &neighbors, Vector<EtherAddress> &known_neighbors, Vector<EtherAddress> &filtered_neighbors);
};

CLICK_ENDDECLS
#endif
