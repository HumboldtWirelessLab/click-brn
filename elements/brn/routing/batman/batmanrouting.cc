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
 * BatmanRouting.{cc,hh}
 */

#include <click/config.h>
#include "batmanrouting.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brn2.h"
#include "batmanroutingtable.hh"
#include "batmanprotocol.hh"


CLICK_DECLS

BatmanRouting::BatmanRouting():
  _brt(NULL),_nodeid(NULL),
  _routeId(0),
  _hop_margin(0)
{
  BRNElement::init();
}

BatmanRouting::~BatmanRouting()
{
}

int
BatmanRouting::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM , cpElement, &_nodeid,
      "BATMANTABLE", cpkP+cpkM , cpElement, &_brt,
      "HOPMARGIN", cpkP, cpUnsignedShort, &_hop_margin,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BatmanRouting::initialize(ErrorHandler *)
{
  return 0;
}

void
BatmanRouting::push( int /*port*/, Packet *packet )
{
  const click_ether *et = reinterpret_cast<const click_ether*>(packet->data());
  EtherAddress dst = EtherAddress(et->ether_dhost);

  if ( _nodeid->isIdentical(&dst) ) {
    BRN_DEBUG("Final destination");
    output(0).push(packet);
    return;
  }

  BatmanRoutingTable::BatmanForwarderEntry *bfe = _brt->getBestForwarder(dst);

  if ( bfe == NULL ) {
    BRN_ERROR("No Forwarder");
    output(2).push(packet);
    return;
  }

  //BRN2Device *dev = _nodeid->getDeviceByIndex(bfe->_recvDeviceId);

  uint8_t ttl = BRNPacketAnno::ttl_anno(packet);
  if ( ttl == 0 ) ttl = bfe->_hops + _hop_margin /*HOP-MARGIN*/;  //TODO: Discard if ttl higher than expected hops ??

  BRN_DEBUG("Start forward. Next hop: %s",bfe->_forwarder.unparse().c_str());
  //int int_ttl = ttl;
  //click_chatter("Wanted ttl: %d",int_ttl);
  if ( ++_routeId == 0 ) _routeId = 1;
  WritablePacket *batroutep = BatmanProtocol::add_batman_routing(packet, 0, _routeId );
  WritablePacket *batp = BatmanProtocol::add_batman_header(batroutep, BATMAN_ROUTING_FORWARD, ttl );

  WritablePacket *brnrp = BRNProtocol::add_brn_header(batp, BRN_PORT_BATMAN, BRN_PORT_BATMAN, ttl, DEFAULT_TOS);

  BRNPacketAnno::set_ether_anno(brnrp, *_nodeid->getMasterAddress(), bfe->_forwarder, ETHERTYPE_BRN);

  output(1).push(brnrp);

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BatmanRouting::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BatmanProtocol)
EXPORT_ELEMENT(BatmanRouting)
