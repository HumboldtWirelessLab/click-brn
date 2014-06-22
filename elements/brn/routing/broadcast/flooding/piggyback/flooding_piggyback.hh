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

#ifndef FLOODING_PIGGYBACK_ELEMENT_HH
#define FLOODING_PIGGYBACK_ELEMENT_HH
#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/broadcast/flooding/flooding.hh"

CLICK_DECLS

/*
 *=c
 *FloodingPiggyback()
 *=s encapsulation, Ethernet
 *encapsulates packets in Ethernet header (information used from Packet::ether_header())
*/
#define BCAST_EXTRA_DATA_LASTNODE_DFL_MAX_NODES        0
#define BCAST_EXTRA_DATA_NEIGHBOURS_UPDATE_INTERVAL 10000

class FloodingPiggyback : public BRNElement {

 public:
  //
  //methods
  //
  FloodingPiggyback();
  ~FloodingPiggyback();

  const char *class_name() const  { return "FloodingPiggyback"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  void add_handlers();

  Packet *simple_action(Packet *);

  static int bcast_header_add_last_nodes(Flooding *fl, FloodingDB *fl_db, EtherAddress *src, uint32_t id, uint8_t *buffer, uint32_t buffer_size, uint32_t max_last_nodes, NetworkGraph &net_graph,
                                         EtherAddress &blacklist); /*TODO: make blackLIST(!!) */
  static int bcast_header_get_last_nodes(Flooding *fl, FloodingDB *fl_db, EtherAddress *src, uint32_t id, uint8_t *rxdata, uint32_t rx_data_size );

  BRN2NodeIdentity *_me;
  Flooding *_flooding;
  FloodingHelper *_fhelper;
  FloodingDB *_flooding_db;

  uint8_t extra_data[BCAST_MAX_EXTRA_DATA_SIZE];
  uint32_t extra_data_size;
  uint32_t _max_last_nodes_per_pkt;
  uint32_t _update_interval;

  Timestamp _last_update;
  NetworkGraph _net_graph;

};

CLICK_ENDDECLS
#endif
