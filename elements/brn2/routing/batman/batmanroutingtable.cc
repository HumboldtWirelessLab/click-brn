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
#include "batmanroutingtable.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"

CLICK_DECLS

BatmanRoutingTable::BatmanRoutingTable()
{
  BRNElement::init();
}

BatmanRoutingTable::~BatmanRoutingTable()
{
}

int
BatmanRoutingTable::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _nodeid = NULL;

  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM , cpElement, &_nodeid,
      "LINKTABLE", cpkP+cpkM , cpElement, &_linktable,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
BatmanRoutingTable::initialize(ErrorHandler *)
{
  return 0;
}

BatmanRoutingTable::BatmanNode*
BatmanRoutingTable::getBatmanNode(EtherAddress addr)
{
  return _nodemap.findp(addr);
}

BatmanRoutingTable::BatmanNode*
BatmanRoutingTable::addBatmanNode(EtherAddress addr)
{
  BatmanRoutingTable::BatmanNode* _node;
  if ( (_node = _nodemap.findp(addr)) != NULL ) return _node;

  _nodemap.insert(addr,BatmanNode(addr));

  return _nodemap.findp(addr);
}


BatmanRoutingTable::BatmanForwarderEntry *
BatmanRoutingTable::getBestForwarder(EtherAddress dst)
{
  BatmanNode *bn;

  if ( (bn = getBatmanNode(dst)) == NULL ) return NULL;

  return bn->getBatmanForwarderEntry(bn->_best_forwarder);
}


void
BatmanRoutingTable::update_originator(EtherAddress src, EtherAddress fwd, uint32_t id,
                                      int hops, uint32_t metric_fwd_src, uint32_t metric_to_fwd)
{
  BatmanRoutingTable::BatmanNode* bn = BatmanRoutingTable::addBatmanNode(src);
  bn->update_originator(id, fwd, metric_fwd_src, metric_to_fwd, hops);
}


void
BatmanRoutingTable::get_nodes_to_be_forwarded(int originator_id, BatmanNodePList *bnl)
{
  for (BatmanNodeMapIter i = _nodemap.begin(); i.live(); i++) {
    BatmanNode *bn = _nodemap.findp(i.key());
    if ( bn->should_be_forwarded(originator_id) ) bnl->push_back(bn);
  }
}

String
BatmanRoutingTable::print_rt()
{
  StringAccum sa;

  sa << "Routing Info" << "\n";
  if ( _nodeid != NULL ) {
    sa << "Address: " << _nodeid->getMasterAddress()->unparse() << " Nodes: " << _nodemap.size() << "\n";
  }

  sa << "Node\t\t\tBest Forwarder\t\tHops\tMetric\tSrcOID\tLastFwdOID\n";

  for (BatmanNodeMapIter i = _nodemap.begin(); i.live(); i++) {
    BatmanNode *bn = _nodemap.findp(i.key());
    BatmanForwarderEntry *bfe = getBestForwarder(bn->_addr);

    if ( bfe == NULL ) {
      sa << bn->_addr.unparse() << "\t00-00-00-00-00-00\t-1\t0\t0\t0\n";
    } else {
      sa << bn->_addr.unparse() << "\t" << bfe->_forwarder.unparse() << "\t" << (int)bfe->_hops << "\t";
      sa << bfe->_metric << "\t" << bn->_latest_originator_id << "\t" << bn->_last_forward_originator_id << "\n" ;
    }
  }

  return sa.take_string();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {
  H_NODES
};

static String
read_param(Element *e, void *thunk)
{
  BatmanRoutingTable *brt = (BatmanRoutingTable *)e;

  switch ((uintptr_t) thunk)
  {
    case H_NODES :
      return ( brt->print_rt() );
  }

  return String();
}

void
BatmanRoutingTable::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("nodes", read_param, H_NODES);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BatmanRoutingTable)
