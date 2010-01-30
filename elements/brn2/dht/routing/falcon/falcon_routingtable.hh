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

#define RT_UPDATE_NONE        0
#define RT_UPDATE_SUCCESSOR   1
#define RT_UPDATE_PREDECESSOR 2
#define RT_UPDATE_FINGERTABLE 3
#define RT_UPDATE_NEIGHBOUR   4
#define RT_UPDATE_ALL         5

#define RT_NONE        0
#define RT_ME          1
#define RT_SUCCESSOR   2
#define RT_PREDECESSOR 3
#define RT_ALL         4
#define RT_FINGERTABLE 5

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

 private:
  void (*_info_func)(void*, int);
  void *_info_obj;

 public:

  int set_update_callback(void (*info_func)(void*,int), void *info_obj) {
    if ( ( info_func == NULL ) || ( info_obj == NULL ) ) return -1;
    else
    {
      _info_func = info_func;
      _info_obj = info_obj;
      return 0;
    }
  }

  void update_callback(int status) {
    (*_info_func)(_info_obj, status);
  }

  String routing_info(void);
  void reset(void);

  /** getter/setter for status of node (e.g. ok, away,...) */
  int setStatus(DHTnode *node, int status);
  int setStatus(EtherAddress *ea, int status);

  int getStatus(DHTnode *node);
  int getStatus(EtherAddress *ea);

  bool isBetterSuccessor(DHTnode *node);
  bool isBetterPredecessor(DHTnode *node);
  bool isInBetween(DHTnode *a, DHTnode *b, DHTnode *c);
  bool isInBetween(DHTnode *a, DHTnode *b, md5_byte_t *c);
  bool isInBetween(DHTnode *a, md5_byte_t *b, DHTnode *c);
  bool isInBetween(md5_byte_t *a, DHTnode *b, DHTnode *c);

  bool isSuccessor(DHTnode *node);
  bool isPredecessor(DHTnode *node);
  bool isBacklog(DHTnode *node);

  int add_node(DHTnode *node);
  int add_node(DHTnode *node, bool is_neighbour);
  int add_node(DHTnode *node, bool is_neighbour, bool want_callback);
  int add_neighbour(DHTnode *node);
  int add_nodes(DHTnodelist *nodes);

  int add_node_in_FT(DHTnode *node, int position);  //add the node also in the list of all nodes
  int set_node_in_FT(DHTnode *node, int position);  //just set the node in the FT (add the pointer)

  DHTnode *find_node(DHTnode *node);
  DHTnode *find_node(DHTnode *node, int *table);
  DHTnode *find_node_in_tables(DHTnode *node, int *table);

// private:
  /**********************************************/
  /*********      T A B L E S       *************/
  /**********************************************/

  DHTnode *_me;

  DHTnode *successor;
  DHTnode *predecessor;

  DHTnode *backlog;               //TODO: Overhang, while update Fingertable. This node is not in the FT since it is more than one round

  DHTnodelist _fingertable;

  int _lastUpdatedPosition;
  void setLastUpdatedPosition(int position);

  DHTnodelist _allnodes;

  bool fix_successor;
  void fixSuccessor(bool fix) { fix_successor = fix; }
  bool isFixSuccessor(void) { return fix_successor; }

  int _debug;

};

CLICK_ENDDECLS
#endif
