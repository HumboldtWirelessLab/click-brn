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
#include <click/vector.hh>

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

#define RT_UPDATE_NONE        0
#define RT_UPDATE_SUCCESSOR   1
#define RT_UPDATE_PREDECESSOR 2
#define RT_UPDATE_FINGERTABLE 3
#define RT_UPDATE_NEIGHBOUR   4
#define RT_UPDATE_ALL         5

/**
 * Used in funtion which determinate the table in which a given node is placed (e.g. find_node)
 * E.g. use RT_ME if the node is me or RT_FINGERTABLE if th enode in the fingertable
 */

#define RT_NONE        0
#define RT_ME          1
#define RT_SUCCESSOR   2
#define RT_PREDECESSOR 3
#define RT_FINGERTABLE 4
#define RT_ALL         5


#define RT_MAX_NODE_AGE 20 /*seconds*/

/**
 TODO: check correct handle of the main-tables:
       - All nodes are inserted into all_nodes
       - node are only delete from all_nodes
       - in other tables only delete the referenz (NOT the Object (DHTnode))
*/

class FalconRoutingTable : public BRNElement
{
 public:

  class CallbackFunction {

   public:
    void (*_info_func)(void*, int);
    void *_info_obj;

    CallbackFunction( void (*info_func)(void*,int), void *info_obj ) {
      _info_func = info_func;
      _info_obj = info_obj;
    }

    ~CallbackFunction(){}
  };

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
   Vector<CallbackFunction*> _callbacklist;
   void update_callback(int status);

 public:

  int add_update_callback(void (*info_func)(void*,int), void *info_obj);

  String routing_info(void);
  String debug_routing_info(void);
  void reset(void);

  /** getter/setter for status of node (e.g. ok, away,...) */
  int setStatus(DHTnode *node, int status);
  int setStatus(EtherAddress *ea, int status);

  int getStatus(DHTnode *node);
  int getStatus(EtherAddress *ea);

  bool isSuccessor(DHTnode *node);
  bool isPredecessor(DHTnode *node);
  bool isBacklog(DHTnode *node);

  bool isBetterSuccessor(DHTnode *node);
  bool isBetterPredecessor(DHTnode *node);

  DHTnode* findBestSuccessor(DHTnode *node, int max_age);

  int add_node(DHTnode *node);
  int add_node(DHTnode *node, bool is_neighbour);
  int add_neighbour(DHTnode *node);
  int add_nodes(DHTnodelist *nodes);

  int add_node_in_FT(DHTnode *node, int position);  //add the node also in the list of all nodes
  int set_node_in_FT(DHTnode *node, int position);  //just set the node in the FT (add the pointer)
  int index_in_FT(DHTnode *node);
  void clear_FT(int start_index);

  int set_node_in_reverse_FT(DHTnode *node, int position);  //just set the node in the reverse FT (add the pointer)
  int index_in_reverse_FT(DHTnode *node);

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

  DHTnode *backlog; //TODO: Backlog, while update Fingertable. This node is not in the FT since it is more than one round

  DHTnodelist _fingertable;

  DHTnodelist _reverse_fingertable;  //nodes, which have this node in the fingertable
                                     //Node at index X has node in the fingertable at position X
  DHTnodelist _allnodes;

  /**
   * index of the next node which has to be updated in the Fingertable. This index is reset to 0
   * by successor_maintainence on successor
   */
  int _lastUpdatedPosition;
  void setLastUpdatedPosition(int position);
  void incLastUpdatedPosition();

  /**
   * fix_successor represent the status of the knowledge about the successor. If it is true, we
   * are sure to know the right successor.
   */
 private:
  bool fix_successor;
  int ping_successor_counter;

 public:
  void fixSuccessor(bool fix) {
    fix_successor = fix;
    if ( ! fix ) ping_successor_counter = 0;
  }

  bool isFixSuccessor(void) { return fix_successor; }  //TODO: rename to "hasFixSuccessor()"

  void inc_successor_counter(void) { ping_successor_counter++; }
  int get_successor_counter(void) { return ping_successor_counter; }

  int _dbg_routing_info;

 private:
  int max_node_age;

  bool _passive_monitoring;

 public:
  int get_max_node_age() { return max_node_age; }

  void set_passive_monitoring(bool pm) { _passive_monitoring = pm; }
  bool is_passive_monitoring() { return _passive_monitoring; }
};

CLICK_ENDDECLS
#endif
