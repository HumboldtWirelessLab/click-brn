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
 * BatmanForwarder.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"

#include "batmanforwarder.hh"
#include "batmanroutingtable.hh"
#include "batmanprotocol.hh"
#include "elements/brn2/brn2.h"

CLICK_DECLS

BatmanForwarder::BatmanForwarder()
{
  BRNElement::init();
}

BatmanForwarder::~BatmanForwarder()
{
}

int
BatmanForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM , cpElement, &_nodeid,
      "BATMANTABLE", cpkP+cpkM , cpElement, &_brt,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
BatmanForwarder::initialize(ErrorHandler *)
{
  return 0;
}

void
BatmanForwarder::push( int /*port*/, Packet *packet )
{
  click_ether *et;
  batman_header *bh;

  uint8_t ttl = BRNPacketAnno::ttl_anno(packet) - 1;
  BRNPacketAnno::set_ttl_anno(packet, ttl);

  bh = BatmanProtocol::get_batman_header(packet);
  et = BatmanProtocol::get_ether_header(packet);
  EtherAddress dst = EtherAddress(et->ether_dhost);

  if ( _nodeid->isIdentical(&dst) ) {
    BatmanProtocol::pull_batman_routing_header(packet);
    if ( BRNProtocol::is_brn_etherframe(packet) ) { //set ttl if payload is brn_packet
      struct click_brn *brnh = BRNProtocol::get_brnheader_in_etherframe(packet);
      brnh->ttl = ttl;
    }
    BRN_DEBUG("Reach final destination.");

    output(0).push(packet);
    return;
  }

  BatmanRoutingTable::BatmanForwarderEntry *bfe = _brt->getBestForwarder(dst);

  if ( bfe == NULL ) {
    BRN_ERROR("No best forwarder.");
    output(2).push(packet);
    return;
  }

  //BRN2Device *dev = _nodeid->getDeviceByIndex(bfe->_recvDeviceId);

  bh->hops--;

  if ( bh->hops == 0 || ttl == 0 ) {
    BRN_ERROR("To many hops (TTL: %d Hops: %d. Route Error.",(int)ttl,(int)bh->hops);
    output(2).push(packet);
    return;
  }

  //int int_ttl = ttl;
  //click_chatter("set ttl: %d",int_ttl);
  BRN_DEBUG("Next hop: %s", bfe->_forwarder.unparse().c_str());

  WritablePacket *brnrp = BRNProtocol::add_brn_header(packet, BRN_PORT_BATMAN, BRN_PORT_BATMAN, ttl, DEFAULT_TOS);

  BRNPacketAnno::set_ether_anno(brnrp, *_nodeid->getMasterAddress(), bfe->_forwarder, ETHERTYPE_BRN);

  output(1).push(brnrp);

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BatmanForwarder::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BatmanForwarder)
