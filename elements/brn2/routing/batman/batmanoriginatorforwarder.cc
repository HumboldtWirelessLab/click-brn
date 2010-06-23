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
 * BatmanOriginatorForwarder.{cc,hh}
 */

#include <click/config.h>
#include "batmanoriginatorforwarder.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/routing/batman/batmanprotocol.hh"
#include "elements/brn2/brn2.h"


CLICK_DECLS

BatmanOriginatorForwarder::BatmanOriginatorForwarder()
  :_sendbuffer_timer(this),
   _debug(0/*BrnLogger::DEFAULT*/)
{
}

BatmanOriginatorForwarder::~BatmanOriginatorForwarder()
{
}

int
BatmanOriginatorForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM , cpElement, &_nodeid,
      "BATMANTABLE", cpkP+cpkM , cpElement, &_brt,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BatmanOriginatorForwarder::initialize(ErrorHandler *)
{
  click_srandom(_nodeid->getMasterAddress()->hashcode());
  _sendbuffer_timer.initialize(this);
  return 0;
}

void
BatmanOriginatorForwarder::run_timer(Timer *)
{
  if ( _packet_queue.size() > 0 ) {
    output(0).push(_packet_queue[0]);
    _packet_queue.erase(_packet_queue.begin());
    if ( _packet_queue.size() > 0 )
      _sendbuffer_timer.schedule_after_msec(click_random() % (QUEUE_DELAY));
  }
}

void
BatmanOriginatorForwarder::push( int/*port*/, Packet *packet )
{
  EtherAddress last_hop;
  EtherAddress src;
  bool newOriginator = false;

  uint8_t devId;

  struct batman_header *bh;
  struct batman_originator *bo;

  bh = BatmanProtocol::get_batman_header(packet);
  bo = BatmanProtocol::get_batman_originator(packet);

  last_hop = BRNPacketAnno::src_ether_anno(packet);
  devId = BRNPacketAnno::devicenumber_anno(packet);
  BRN2Device *dev = _nodeid->getDeviceByIndex(devId);

  src = EtherAddress(bo->src);

  if ( src ==  EtherAddress(dev->getEtherAddress()->data())) {
//    click_chatter("Got my OriginatorMsg. Kill it!");
    packet->kill();
    return;
  }

  newOriginator = _brt->isNewOriginator(ntohl(bo->id), src);
  _brt->handleNewOriginator(ntohl(bo->id), src, last_hop, devId, bh->hops);

  /** check, if src is a neighbour and add him if so */
  if ( bh->hops == ORIGINATOR_SRC_HOPS ) {
    _brt->addBatmanNeighbour(src);
  }

  if ( newOriginator ) {
    //click_chatter("Originator is new. Forward !");
    bh->hops++;
    WritablePacket *p_fwd = BRNProtocol::add_brn_header(packet, BRN_PORT_BATMAN, BRN_PORT_BATMAN, 10, DEFAULT_TOS);
//    BRNPacketAnno::set_ether_anno(p_fwd, dev->getEtherAddress()->data(), brn_ethernet_broadcast, ETHERTYPE_BRN);
    BRNPacketAnno::set_ether_anno(p_fwd, EtherAddress(dev->getEtherAddress()->data()),
                                  EtherAddress(broadcast), ETHERTYPE_BRN);

    /*Now using queue instead of output(0).push(p_fwd); to reduce collision in simulations*/
    _packet_queue.push_back(p_fwd);
    _sendbuffer_timer.schedule_after_msec(click_random() % ( QUEUE_DELAY ));

  } else {
    //click_chatter("Originator already forward.");
    packet->kill();
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BatmanOriginatorForwarder *fl = (BatmanOriginatorForwarder *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BatmanOriginatorForwarder *fl = (BatmanOriginatorForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
BatmanOriginatorForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BatmanOriginatorForwarder)
