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

/*
 * BatmanRoutingTable.{cc,hh}
 */

#include <click/config.h>
#include "elements/brn/common.hh"

#include "batmanroutingtable.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"


CLICK_DECLS

BatmanRoutingTable::BatmanRoutingTable()
  :_debug(BrnLogger::DEFAULT)
{
}

BatmanRoutingTable::~BatmanRoutingTable()
{
}

int
BatmanRoutingTable::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BatmanRoutingTable::initialize(ErrorHandler *)
{
  return 0;
}

BatmanRoutingTable::BatmanNode *
BatmanRoutingTable::getBatmanNode(EtherAddress addr)
{
  BatmanNode *bn;

  for ( int i = 0; i < _nodelist.size(); i++ )
  {
    bn = &_nodelist[i];
    if ( bn->_addr == addr ) {
      return bn;
    }
  }

  return NULL;
}

int
BatmanRoutingTable::addBatmanNode(EtherAddress addr)
{
  _nodelist.push_back(BatmanNode(addr));
}


void
BatmanRoutingTable::handleNewOriginator(uint32_t id, EtherAddress src, EtherAddress fwd, int hops)
{
  BatmanNode *bn;

  bn = getBatmanNode(src);
  if ( bn == NULL ) {
    addBatmanNode(src);
    bn = getBatmanNode(src);
    if ( bn == NULL ) click_chatter("Not able to add new Node");
  }

  bn->add_originator( fwd, id, hops );
}

String
BatmanRoutingTable::infoGetNodes()
{
  StringAccum sa;

  sa << "Routing Info" << "\n";
  for ( int i = 0; i < _nodelist.size(); i++ ) {
    sa << " " << _nodelist[i]._addr.s() << "\t" << _nodelist[i]._last_originator_id << "\n";

    for ( int j = 0; j < _nodelist[i]._forwarder.size(); j++ ) {
      sa << "   Forwarder " << j << ": " << _nodelist[i]._forwarder[j]._addr.s() << "\t" << _nodelist[i]._forwarder[j]._oil.size() << "\n";
    }
  }

  return sa.take_string();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {
  H_NODES,
  H_DEBUG
};

static String
read_debug_param(Element *e, void *thunk)
{
  BatmanRoutingTable *brt = (BatmanRoutingTable *)e;

  switch ((uintptr_t) thunk)
  {
    case H_NODES :
      return ( brt->infoGetNodes() );
    case H_DEBUG :
      return String(brt->_debug) + "\n";
    default: return String();
  }

  return String(brt->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BatmanRoutingTable *fl = (BatmanRoutingTable *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
BatmanRoutingTable::add_handlers()
{
  add_read_handler("debug", read_debug_param, H_DEBUG);
  add_read_handler("nodes", read_debug_param, H_NODES);
  add_write_handler("debug", write_debug_param, H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BatmanRoutingTable)
