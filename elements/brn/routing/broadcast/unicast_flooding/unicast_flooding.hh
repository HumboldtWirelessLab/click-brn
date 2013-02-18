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

#ifndef UNICAST_FLOODING_ELEMENT_HH
#define UNICAST_FLOODING_ELEMENT_HH

#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/routing/broadcast/flooding/flooding.hh"


CLICK_DECLS

/*
 * =c
 * UnicastFlooding()
 * =s converts a broadcast packet into a unicast packet
 * output: rewrited packet
 * =d
 *
 * Restrictions: works only together with ETX metric
 */

#define UNICAST_FLOODING_NO_REWRITE 0
#define UNICAST_FLOODING_STATIC_REWRITE 5
#define UNICAST_FLOODING_ALL_UNICAST 6


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

class UnicastFlooding : public BRNElement {

 public:
  //
  //methods
  //
  UnicastFlooding();
  ~UnicastFlooding();

  const char *class_name() const  { return "UnicastFlooding"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  void push(int, Packet *);

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

public: 
  //
  //member
  //
  BRN2NodeIdentity *_me;
  Flooding *_flooding;
  Brn2LinkTable *_link_table;

private:
  int _max_metric_to_neighbor; // max. metric towards a neighbor
  int _cand_selection_strategy; // the way we choose the candidate for unicast forwarding
  EtherAddress static_dst_mac;

  bool algorithm_1(EtherAddress &next_hop, Vector<EtherAddress> &neighbors);
  bool algorithm_2(EtherAddress &next_hop, Vector<EtherAddress> &neighbors);
  bool algorithm_3(EtherAddress &next_hop, Vector<EtherAddress> &neighbors);

  // helper
  void get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out);
  int subtract_and_cnt(const Vector<EtherAddress> &s1, const Vector<EtherAddress> &s2);
  void addAll(const Vector<EtherAddress> &newS, Vector<EtherAddress> &inout);

  int findWorst(const EtherAddress &src, Vector<EtherAddress> &neighbors);
  void filter_bad_one_hop_neighbors(const EtherAddress &node, Vector<EtherAddress> &neighbors, Vector<EtherAddress> &filtered_neighbors);
  void filter_known_one_hop_neighbors(const EtherAddress &node, Vector<EtherAddress> &neighbors, Vector<EtherAddress> &filtered_neighbors);
  
 public:

  void set_strategy(int s) { _cand_selection_strategy = s; }
  int get_strategy() { return _cand_selection_strategy; }

  void set_static_mac(EtherAddress *mac) { static_dst_mac = *mac; }
  EtherAddress *get_static_mac() { return &static_dst_mac; }
  
  EtherAddress last_unicast_used;
  uint32_t no_rewrites;
};

CLICK_ENDDECLS
#endif
