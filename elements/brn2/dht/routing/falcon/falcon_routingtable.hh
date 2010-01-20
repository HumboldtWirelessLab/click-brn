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

/* Sender-/Receivernumbers */
/* field: sender,receiver */
#ifndef FALCON_ROUTINGTABLE_HH
#define FALCON_ROUTINGTABLE_HH

#include <click/element.hh>

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"

CLICK_DECLS

class FalconRoutingTable : public Element
{

 public:
  FalconRoutingTable();
  ~FalconRoutingTable();

  const char *class_name() const  { return "FalconRoutingTable"; }

  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  String routing_info(void);
  void reset(void);

  /** getter/setter for status of node (e.g. ok, away,...) */
  int setStatus(DHTnode *node, int status);
  int setStatus(EtherAddress *ea, int status);

  int getStatus(DHTnode *node);
  int getStatus(EtherAddress *ea);

  bool isBetterSuccessor(DHTnode *node);
  bool isBetterPredecessor(DHTnode *node);


  void add_node(DHTnode *node);
  void add_node(DHTnode *node, bool is_neighbour);
  void add_neighbour(DHTnode *node);
  void add_nodes(DHTnodelist *nodes);

// private:
  DHTnode *_me;

  DHTnode *successor;
  DHTnode *predecessor;

  DHTnodelist _fingertable;

  DHTnodelist _foreignnodes;

};

CLICK_ENDDECLS
#endif
