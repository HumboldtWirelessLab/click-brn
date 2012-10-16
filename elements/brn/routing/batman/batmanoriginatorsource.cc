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
 * BatmanOriginatorSource.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn/routing/identity/brn2_device.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/routing/batman/batmanprotocol.hh"
#include "elements/brn/brn2.h"

#include "batmanoriginatorsource.hh"

CLICK_DECLS

BatmanOriginatorSource::BatmanOriginatorSource()
  :  _send_timer(static_send_timer_hook,this)
{
  BRNElement::init();
}

BatmanOriginatorSource::~BatmanOriginatorSource()
{
}

int
BatmanOriginatorSource::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "BATMANTABLE", cpkP+cpkM , cpElement, &_brt,
      "NODEID", cpkP+cpkM , cpElement, &_nodeid,
      "INTERVAL", cpkP+cpkM , cpInteger, &_interval,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BatmanOriginatorSource::initialize(ErrorHandler *)
{
  click_srandom(_nodeid->getMasterAddress()->hashcode());

  _id = 0;
  _send_timer.initialize(this);
  _send_timer.schedule_after_msec(click_random() % (_interval ) );

  return 0;
}

void
BatmanOriginatorSource::set_send_timer()
{
  _send_timer.schedule_after_msec( _interval );
}

void
BatmanOriginatorSource::static_send_timer_hook(Timer *t, void *bos)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((BatmanOriginatorSource*)bos)->sendOriginator();
  ((BatmanOriginatorSource*)bos)->set_send_timer();
}

void
BatmanOriginatorSource::sendOriginator()
{
  _id++;

  BatmanRoutingTable::BatmanNodePList bnl;
  BRN2Device *dev = _nodeid->getDeviceByIndex(0);
  int num_neighbours = 0;

  if ( _brt->_originator_mode == BATMAN_ORIGINATORMODE_COMPRESSED ) {
    /*Compressed Version*/
    _brt->get_nodes_to_be_forwarded(_id, &bnl);
    num_neighbours = bnl.size();
    /*End Compressed Version*/
  }

  WritablePacket *p = BatmanProtocol::new_batman_originator(_id, 0, num_neighbours);

  if ( _brt->_originator_mode == BATMAN_ORIGINATORMODE_COMPRESSED ) {
    /*Compressed Version*/
    struct batman_node s_bn;
    for ( int i = 0; i < bnl.size(); i++ ) {
      BatmanRoutingTable::BatmanNode *bn = bnl[i];

      s_bn.id = htonl(bn->_latest_originator_id);
      s_bn.metric = htons(bn->_best_metric);
      s_bn.flags = 0;
      s_bn.hops = bn->_hops_best_metric;
      memcpy(s_bn.src, bn->_addr.data(), 6);

      BatmanProtocol::add_batman_node(p, &s_bn, i);
      bn->forward_node(_id);
    }
    /*End Compressed Version*/
  }

  WritablePacket *p_brn = BRNProtocol::add_brn_header(p, BRN_PORT_BATMAN, BRN_PORT_BATMAN,
                                                         BATMAN_MAX_ORIGINATOR_HOPS, DEFAULT_TOS);

  BRNPacketAnno::set_ether_anno(p_brn, dev->getEtherAddress()->data(), brn_ethernet_broadcast, ETHERTYPE_BRN);

  output(0).push(p_brn);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BatmanOriginatorSource::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BatmanProtocol)
EXPORT_ELEMENT(BatmanOriginatorSource)
