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
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "falcon_functions.hh"
#include "falcon_routingtable.hh"

CLICK_DECLS

FalconRoutingTable::FalconRoutingTable():
  successor(NULL),
  predecessor(NULL),
  backlog(NULL),
  _lastUpdatedPosition(0),
  fix_successor(false),
  _debug(BrnLogger::DEFAULT)
{
}

FalconRoutingTable::~FalconRoutingTable()
{
}

int
FalconRoutingTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  EtherAddress _my_ether_addr;
  bool use_md5 = true;

  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_my_ether_addr,
      "USEMD5", cpkN, cpBool, &use_md5,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

   if ( use_md5 )
    _me = new DHTnode(_my_ether_addr);
   else {
     click_chatter("Don't use md5");
     md5_byte_t md5_digest[MAX_NODEID_LENTGH];
     memset(md5_digest,0,MAX_NODEID_LENTGH);
     md5_digest[0] = _my_ether_addr.data()[4];
     _me = new DHTnode(_my_ether_addr,md5_digest);
   }

  _me->_status = STATUS_OK;
  _me->_neighbor = true;

  return 0;
}

int
FalconRoutingTable::initialize(ErrorHandler *)
{
  return 0;
}

bool
FalconRoutingTable::isSuccessor(DHTnode *node)
{
  if ( successor == NULL ) return false;
  return ( node->equals(successor) );
}

bool
FalconRoutingTable::isPredecessor(DHTnode *node)
{
  if ( predecessor == NULL ) return false;
  return ( node->equals(predecessor) );
}

bool
FalconRoutingTable::isBacklog(DHTnode *node)
{
  if ( backlog == NULL ) return false;
  return ( node->equals(backlog) );
}

bool
FalconRoutingTable::isBetterSuccessor(DHTnode *node)
{
  if ( successor == NULL ) return true;
  return FalconFunctions::is_in_between( _me, successor, node);
}

bool
FalconRoutingTable::isBetterPredecessor(DHTnode *node)
{
  if ( predecessor == NULL ) return true;
  return FalconFunctions::is_in_between( predecessor, _me, node);
}

int
FalconRoutingTable::add_node(DHTnode *node)
{
  DHTnode *n;

  n = _allnodes.get_dhtnode(node);

  if (n == NULL) {                     //add node if node is not in list
    if ( node->_digest_length != 0 ) { //and new node has a valid node_id
      n = node->clone();
      _allnodes.add_dhtnode(n);
    } else {                           //if no valid node_id: finish here
      return 0;
    }
  } else {
    n->_status = node->_status;
    n->set_age(&(node->_age));    //update node
    n->set_nodeid(node->_md5_digest);
    return 0;                     //get back since there is no new node (succ and pred will not changed)
  }

  if ( isBetterSuccessor(n) ) {

    if ( predecessor == NULL ) {
      predecessor = n;
      update_callback(RT_UPDATE_PREDECESSOR);  //TODO: place this anywhere else. the add_node-function is
                                              //       called by complex function, so it can result in problems
    }

    successor = n;
    fixSuccessor(false);                       //new succ. check him.

    set_node_in_FT(successor, 0);
    update_callback(RT_UPDATE_SUCCESSOR);      //TODO: place this anywhere else. the add_node-function is
                                               //      called by complex function, so it can result in problems

  } else if ( isBetterPredecessor(n) ) {

    if ( successor == NULL ) {
      successor = n;
      set_node_in_FT(successor, 0);
      update_callback(RT_UPDATE_SUCCESSOR);      //TODO: place this anywhere else. the add_node-function is
                                                 //      called by complex function, so it can result in problems
    }

    predecessor = n;
    update_callback(RT_UPDATE_PREDECESSOR);  //TODO: place this anywhere else. the add_node-function is
                                             //      called by complex function, so it can result in problems
  }

  return 0;
}

int
FalconRoutingTable::add_node(DHTnode *node, bool is_neighbour)
{
  node->_neighbor = is_neighbour;
  return add_node(node);
}

int
FalconRoutingTable::add_neighbour(DHTnode *node)
{
  return add_node(node, true);
}

int
FalconRoutingTable::add_nodes(DHTnodelist *nodes)
{
  DHTnode *n;

  for ( int i = 0; i < nodes->size(); i++ ) {
    n = nodes->get_dhtnode(i);
    add_node(n);
  }

  return 0;
}

int
FalconRoutingTable::add_node_in_FT(DHTnode *node, int position)
{
  int table;
  DHTnode *fn;

  add_node(node); //add node to known nodes or rather update it

  if ( isSuccessor(node) && (position != 0) ) {
    BRN_DEBUG("Node is successor and so position should be 0 and not %d",position);
    return 0;
  }

  fn = find_node_in_tables(node, &table);

  if ( table != RT_FINGERTABLE ) {
    set_node_in_FT(node, position);
  } else {
    BRN_DEBUG("Check node position in FT and update the node");
  }

  return 0;
}

void
FalconRoutingTable::setLastUpdatedPosition(int position) {
  if ( ( position < _fingertable.size() ) && ( position < _lastUpdatedPosition ) )  _lastUpdatedPosition = position;
}

void
FalconRoutingTable::incLastUpdatedPosition(void)
{
  _lastUpdatedPosition = (_lastUpdatedPosition + 1) % _fingertable.size();
}

int
FalconRoutingTable::set_node_in_FT(DHTnode *node, int position)
{
  if ( _fingertable.size() < position ) {
    BRN_ERROR("FT too small. Discard Entry.");
  } else {
    if ( _fingertable.size() == position ) {
      _fingertable.add_dhtnode(node);
    } else {
      _fingertable.swap_dhtnode(node, position);      //replace node in FT, but don't delete the old one, since it
                                                      //is in the all_nodes_table
    }

    setLastUpdatedPosition(position);                 //Fingertable is updated, so we should update also the rest,
                                                      // which depends on this change
                                                      //TODO: move this to function which calls this. Successor_maint
  }                                                   //      causes that higher table entries aren't updated
  return 0;
}

int
FalconRoutingTable::index_in_FT(DHTnode *node)
{
  return _fingertable.get_index_dhtnode(node);
}

void
FalconRoutingTable::clear_FT(int start_index)
{
  _fingertable.clear(start_index, _fingertable.size() - 1);
}


int
FalconRoutingTable::set_node_in_reverse_FT(DHTnode *node, int position)
{
  int ind = _reverse_fingertable.get_index_dhtnode(node);
  if ( ( ind != -1 ) && ( ind != position ) ) {
    if ( position < ind ) {
      DHTnode *old_pos = _reverse_fingertable.swap_dhtnode(node, position);
      _reverse_fingertable.swap_dhtnode(old_pos, ind);
    }
  } else {
    if ( _reverse_fingertable.size() < position ) {
      BRN_DEBUG("Reverse FT too small. Discard Entry.");
    } else {
      if ( _reverse_fingertable.size() == position ) {
        _reverse_fingertable.add_dhtnode(node);
      } else {
        _reverse_fingertable.swap_dhtnode(node, position); //replace node in reverse FT, but don't delete the old one,
                                                           //since it is in the all_nodes_table
      }
    }
  }
  return 0;
}

int
FalconRoutingTable::index_in_reverse_FT(DHTnode *node)
{
  return _reverse_fingertable.get_index_dhtnode(node);
}

/*************************************************************************************************/
/******************************* F I N D   N O D E ***********************************************/
/*************************************************************************************************/

DHTnode *
FalconRoutingTable::find_node_in_tables(DHTnode *node, int *table)
{
  DHTnode *n;

  n = _fingertable.get_dhtnode(node);
  if ( n != NULL ) {
    *table = RT_FINGERTABLE;
    return n;
  }

  n = _allnodes.get_dhtnode(node);
  if ( n != NULL ) {
    *table = RT_ALL;
    return n;
  }

  *table = RT_NONE;
  return NULL;
}

DHTnode *
FalconRoutingTable::find_node(DHTnode *node, int *table)
{
  DHTnode *n;

  if ( node->equals(_me) ) {
    *table = RT_ME;
    return _me;
  }

  if ( node->equals(successor) ) {
    *table = RT_SUCCESSOR;
    return successor;
  }

  if ( node->equals(predecessor) ) {
    *table = RT_PREDECESSOR;
    return predecessor;
  }

  n = _fingertable.get_dhtnode(node);
  if ( n != NULL ) {
    *table = RT_FINGERTABLE;
    return n;
  }

  n = _allnodes.get_dhtnode(node);
  if ( n != NULL ) {
    *table = RT_ALL;
    return n;
  }

  *table = RT_NONE;
  return NULL;
}

DHTnode *
FalconRoutingTable::find_node(DHTnode *node)
{
  int table;
  return find_node(node, &table);
}

void
FalconRoutingTable::reset()
{
  _fingertable.clear();
  _reverse_fingertable.clear();
  _allnodes.del();

  successor = NULL;
  predecessor = NULL;
  backlog = NULL;

  fix_successor = false;
}

/*************************************************************************************************/
/******************************** C A L L B A C K ************************************************/
/*************************************************************************************************/
int
FalconRoutingTable::add_update_callback(void (*info_func)(void*,int), void *info_obj)
{
  _callbacklist.push_back(new CallbackFunction(info_func, info_obj));
  return 0;
}

void
FalconRoutingTable::update_callback(int status)
{
  CallbackFunction *cbf;
  for ( int i = 0; i < _callbacklist.size(); i++ ) {
    cbf = _callbacklist[i];

    (*cbf->_info_func)(cbf->_info_obj, status);
  }
}

/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

String
FalconRoutingTable::routing_info(void)
{
  StringAccum sa;
  DHTnode *node;
  uint32_t numberOfNodes = 0;
  char digest[16*2 + 1];

  numberOfNodes = _allnodes.size();

  MD5::printDigest(_me->_md5_digest, digest);
  sa << "Routing Info ( Node: " << _me->_ether_addr.unparse() << "\t" << digest << " )\n";
  sa << "DHT-Nodes (" << (int)numberOfNodes  << ") :\n";

  if ( successor != NULL ) {
    MD5::printDigest(successor->_md5_digest, digest);
    sa << "Successor: " << successor->_ether_addr.unparse() << "\t" << digest << "\t" << isFixSuccessor() << "\n";
  } else {
    sa << "Successor: xx:xx:xx:xx:xx:xx\txxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\t(false)\n";
  }

  if ( predecessor != NULL ) {
    MD5::printDigest(predecessor->_md5_digest, digest);
    sa << "Predecessor: " << predecessor->_ether_addr.unparse() << "\t" << digest << "\n";
  }

  if ( backlog != NULL ) {
    MD5::printDigest(backlog->_md5_digest, digest);
    sa << "Backlog: " << backlog->_ether_addr.unparse() << "\t" << digest << "\n";
  }

  sa << "\nFingertable (" << _fingertable.size() << ") :\n";
  sa << "Etheraddress\t\tNode-ID\t\t\t\t\tNeighbour\tStatus\tAge\t\tLast Ping\n";
  for( int i = 0; i < _fingertable.size(); i++ )
  {
    node = _fingertable.get_dhtnode(i);

    sa << node->_ether_addr.unparse();
    MD5::printDigest(node->_md5_digest, digest);

    sa << "\t" << digest;
    if ( node->_neighbor )
      sa << "\ttrue";
    else
      sa << "\tfalse";

    sa << "\t\t" << (int)node->_status;
    sa << "\t" << node->get_age();
    sa << "\t" << node->get_last_ping();

    sa << "\n";
  }

  sa << "\nReverse Fingertable (" << _reverse_fingertable.size() << ") :\n";
  sa << "Etheraddress\t\tNode-ID\t\t\t\t\tNeighbour\tStatus\tAge\t\tLast Ping\n";
  for( int i = 0; i < _reverse_fingertable.size(); i++ )
  {
    node = _reverse_fingertable.get_dhtnode(i);

    sa << node->_ether_addr.unparse();
    MD5::printDigest(node->_md5_digest, digest);

    sa << "\t" << digest;
    if ( node->_neighbor )
      sa << "\ttrue";
    else
      sa << "\tfalse";

    sa << "\t\t" << (int)node->_status;
    sa << "\t" << node->get_age();
    sa << "\t" << node->get_last_ping();

    sa << "\n";
  }

  sa << "\nAll nodes (" << _allnodes.size() << ") :\n";
  sa << "Etheraddress\t\tNode-ID\t\t\t\t\tNeighbour\tStatus\tAge\t\tLast Ping\n";
  for( int i = 0; i < _allnodes.size(); i++ )
  {
    node = _allnodes.get_dhtnode(i);

    sa << node->_ether_addr.unparse();
    MD5::printDigest(node->_md5_digest, digest);

    sa << "\t" << digest;
    if ( node->_neighbor )
      sa << "\ttrue";
    else
      sa << "\tfalse";

    sa << "\t\t" << (int)node->_status;
    sa << "\t" << node->get_age();
    sa << "\t" << node->get_last_ping();
    sa << "\n";
  }

  return sa.take_string();
}

enum {
  H_ROUTING_INFO
};

static String
read_param(Element *e, void *thunk)
{
  FalconRoutingTable *dht_falcon = (FalconRoutingTable *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_falcon->routing_info( ) );
    default: return String();
  }
}

void
FalconRoutingTable::add_handlers()
{
  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingTable)
