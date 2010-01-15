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
#include <click/etheraddress.hh>

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
  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_me,
      cpEnd) < 0)
    return -1;

  return 0;
}


/*
String
    DHTRoutingFalcon::routing_info(void)
{
  StringAccum sa;
  DHTnode *node;
  uint32_t numberOfNodes = 0;
  char digest[16*2 + 1];

  numberOfNodes = _fingertable.size();
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

  return sa.take_string();
}


enum {
  H_ROUTING_INFO
};

static String
    read_param(Element *e, void *thunk)
{
  DHTRoutingFalcon *dht_falcon = (DHTRoutingFalcon *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_falcon->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingFalcon::add_handlers()
{
  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}
*/
CLICK_ENDDECLS
ELEMENT_PROVIDES(FalconRoutingTable)
