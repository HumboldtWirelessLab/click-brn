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

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "falcon_routingtable.hh"

CLICK_DECLS

FalconRoutingTable::FalconRoutingTable():
  successor(NULL),
  predecessor(NULL)
{
}

FalconRoutingTable::~FalconRoutingTable()
{
}

int
FalconRoutingTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  EtherAddress _my_ether_addr;

  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_my_ether_addr,
      cpEnd) < 0)
    return -1;

  _me = new DHTnode(_my_ether_addr);
  _me->_status = STATUS_OK;
  _me->_neighbor = true;

  return 0;
}

int
FalconRoutingTable::initialize(ErrorHandler *)
{
  return 0;
}


void
FalconRoutingTable::add_node(DHTnode *node)
{
  DHTnode *n, *nc;

  if ( node->equals(successor) ) {
    //update node
  } else {
    if ( node->equals(predecessor) ) {
      //update node
    } else {
      n = _fingertable.get_dhtnode(node);

      if ( n == NULL ) {
        n = _foreignnodes.get_dhtnode(node);

        if ( n == NULL ) {                                //node is new so insert it, but check were it fits best (foreign, succ, pre,...)
          nc = node->clone();

          //check node , wheter it fits to succ, pre ....
          if ( isBetterSuccessor(nc) ) {

            if ( predecessor == NULL ) predecessor = nc;
            else if ( ! successor->equals(predecessor) ) _foreignnodes.add_dhtnode(successor);           //add only if current successor IS NOT also current predecessor

            successor = nc;

          } else if ( isBetterPredecessor(nc) ) {

            if ( successor == NULL ) successor = nc;
            else if ( ! predecessor->equals(successor) ) _foreignnodes.add_dhtnode(predecessor);         //add only if current predecessor IS NOT also current successor

            predecessor = nc;

          } else {
            //if not: add as foreign
            _foreignnodes.add_dhtnode(nc);
          }
        } else {
          //update node
        }
      } else {
        //update node
      }
    }
  }
}

bool
FalconRoutingTable::isBetterSuccessor(DHTnode *node)
{
  if ( successor == NULL ) return true;

  if ( MD5::hexcompare( successor->_md5_digest, _me->_md5_digest ) >= 0 ) {
    return ( ( MD5::hexcompare( successor->_md5_digest, node->_md5_digest ) >= 0 ) && ( MD5::hexcompare( node->_md5_digest, _me->_md5_digest ) >= 0 ) );
  }

  return ( ( MD5::hexcompare( successor->_md5_digest, node->_md5_digest ) >= 0 ) || ( MD5::hexcompare( node->_md5_digest, _me->_md5_digest ) >= 0 ) );
}

bool
FalconRoutingTable::isBetterPredecessor(DHTnode *node)
{
  if ( predecessor == NULL ) return true;

  if ( MD5::hexcompare( _me->_md5_digest, predecessor->_md5_digest ) >= 0 ) {
    return ( ( MD5::hexcompare( node->_md5_digest, predecessor->_md5_digest ) >= 0 ) && ( MD5::hexcompare(_me->_md5_digest, node->_md5_digest ) >= 0 ) );
  }

  return ( ( MD5::hexcompare( node->_md5_digest, predecessor->_md5_digest ) >= 0 ) || ( MD5::hexcompare(_me->_md5_digest, node->_md5_digest ) >= 0 ) );
}

void
FalconRoutingTable::add_node(DHTnode *node, bool is_neighbour)
{
  node->_neighbor = is_neighbour;
  add_node(node);
}

void
FalconRoutingTable::add_neighbour(DHTnode *node)
{
  add_node(node, true);
}

void
FalconRoutingTable::add_nodes(DHTnodelist *nodes)
{
  DHTnode *n;

  for ( int i = 0; i < nodes->size(); i++ ) {
    n = nodes->get_dhtnode(i);
    add_node(n);
  }
}

void
FalconRoutingTable::reset()
{
  _fingertable.del();
  _foreignnodes.del();

  delete successor;
  delete predecessor;
}

String
FalconRoutingTable::routing_info(void)
{
  StringAccum sa;
  DHTnode *node;
  uint32_t numberOfNodes = 0;
  char digest[16*2 + 1];

  numberOfNodes = _fingertable.size();
  numberOfNodes += _foreignnodes.size();
  if ( successor != NULL ) numberOfNodes++;
  if ( predecessor != NULL ) numberOfNodes++;

  MD5::printDigest(_me->_md5_digest, digest);
  sa << "Routing Info ( Node: " << _me->_ether_addr.unparse() << "\t" << digest << " )\n";
  sa << "DHT-Nodes (" << (int)numberOfNodes  << ") :\n";

  if ( successor != NULL ) {
    MD5::printDigest(successor->_md5_digest, digest);
    sa << "Successor: " << successor->_ether_addr.unparse() << "\t" << digest << "\n";
  }

  if ( predecessor != NULL ) {
    MD5::printDigest(predecessor->_md5_digest, digest);
    sa << "Predecessor: " << predecessor->_ether_addr.unparse() << "\t" << digest << "\n";
  }

  sa << "Fingertable:\n";
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

    sa << "\t" << (int)node->_status;
    sa << "\t" << node->_age;
    sa << "\t" << node->_last_ping;

    sa << "\n";
  }

  sa << "Foreigntable:\n";
  for( int i = 0; i < _foreignnodes.size(); i++ )
  {
    node = _foreignnodes.get_dhtnode(i);

    sa << node->_ether_addr.unparse();
    MD5::printDigest(node->_md5_digest, digest);

    sa << "\t" << digest;
    if ( node->_neighbor )
      sa << "\ttrue";
    else
      sa << "\tfalse";

    sa << "\t" << (int)node->_status;
    sa << "\t" << node->_age;
    sa << "\t" << node->_last_ping;
//    sa << "\t" << isBetterSuccessor(node);
//    sa << "\t" << isBetterPredecessor(node);
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
