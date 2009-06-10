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
#include "elements/brn/common.hh"

#include "batmanoriginatorforwarder.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/routing/batman/batmanprotocol.hh"


CLICK_DECLS

BatmanOriginatorForwarder::BatmanOriginatorForwarder()
  :_debug(BrnLogger::DEFAULT)
{
}

BatmanOriginatorForwarder::~BatmanOriginatorForwarder()
{
}

int
BatmanOriginatorForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_ether_addr,  //TODO: replace by nodeid and send paket to each (wireless) device
      "BATMANTABLE", cpkP+cpkM , cpElement, &_brt,  //TODO: replace by nodeid and send paket to each (wireless) device
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BatmanOriginatorForwarder::initialize(ErrorHandler *)
{
  return 0;
}

void
BatmanOriginatorForwarder::push( int port, Packet *packet )
{
  EtherAddress last_hop;
  EtherAddress src;

  struct batman_header *bh;
  struct batman_originator *bo;

  bh = BatmanProtocol::get_batman_header(packet);
  bo = BatmanProtocol::get_batman_originator(packet);

  last_hop = BRNPacketAnno::src_ether_anno(packet);
  src = EtherAddress(bo->src);

  click_chatter("got bo from %s forwarded by %s , Hopcount: %d Id: %d",src.s().c_str(),last_hop.s().c_str(),
                                                                       bh->hops, ntohl(bo->id));
  _brt->handleNewOriginator(ntohl(bo->id), src, last_hop, bh->hops);
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
