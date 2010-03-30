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
#include "batmanforwarder.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "batmanroutingtable.hh"
#include "batmanprotocol.hh"
#include "elements/brn2/brn2.h"

CLICK_DECLS

BatmanForwarder::BatmanForwarder()
  :_debug(0/*BrnLogger::DEFAULT*/)
{
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

  bh = BatmanProtocol::get_batman_header(packet);
  et = BatmanProtocol::get_ether_header(packet);
  EtherAddress dst = EtherAddress(et->ether_dhost);

  if ( _nodeid->isIdentical(&dst) ) {
    BatmanProtocol::rm_batman_routing_header(packet);
    output(0).push(packet);
    return;
  }

  BatmanRoutingTable::BatmanForwarderEntry *bfe = _brt->getBestForwarder(dst);

  if ( bfe == NULL ) {
    output(2).push(packet);
    return;
  }

  BRN2Device *dev = _nodeid->getDeviceByIndex(bfe->_recvDeviceId);

  bh->hops--;
  WritablePacket *brnrp = BRNProtocol::add_brn_header(packet, BRN_PORT_BATMAN, BRN_PORT_BATMAN, 10/*TTL*/, DEFAULT_TOS);

  BRNPacketAnno::set_ether_anno(brnrp, EtherAddress(dev->getEtherAddress()->data()), bfe->_addr, 0x8680);

  output(1).push(brnrp);

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BatmanForwarder *fl = (BatmanForwarder *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BatmanForwarder *fl = (BatmanForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
BatmanForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BatmanForwarder)
