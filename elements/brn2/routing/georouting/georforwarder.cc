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
 * GeorForwarder.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"

#include "georforwarder.hh"
#include "geortable.hh"
#include "georprotocol.hh"

CLICK_DECLS

GeorForwarder::GeorForwarder()
  :_debug(BrnLogger::DEFAULT)
{
}

GeorForwarder::~GeorForwarder()
{
}

int
GeorForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM , cpElement, &_nodeid,
      "GEORTABLE", cpkP+cpkM , cpElement, &_rt,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
GeorForwarder::initialize(ErrorHandler *)
{
  return 0;
}

void
GeorForwarder::push( int port, Packet *packet )
{
  if ( port == 0 ) {                              // to forward
    struct geor_header *georh = GeorProtocol::getRoutingHeader(packet);

    EtherAddress dea = EtherAddress(georh->dst);    //get destination
    EtherAddress sea = EtherAddress(georh->src);    //get source

    GPSPosition dpos = GPSPosition(&(georh->dst_pos));
    GPSPosition spos = GPSPosition(&(georh->src_pos));

    BRN_DEBUG("Forward packet from %s to %s",spos.unparse_coord().c_str(),dpos.unparse_coord().c_str());

    _rt->updateEntry(&sea,&(georh->src_pos));
    _rt->updateEntry(&dea,&(georh->dst_pos));  //TODO: check for positionrouting only

    BRN_INFO("Forwarder (%s): %s to %s",_nodeid->getMasterAddress()->unparse().c_str(),
             sea.unparse().c_str(),dea.unparse().c_str());

    uint8_t ttl = BRNPacketAnno::ttl_anno(packet) - 1;
    BRNPacketAnno::set_ttl_anno(packet, ttl);

    BRN_INFO("Set ttl %d",(int)ttl);

    if ( ! _nodeid->isIdentical(&dea) ) {
      EtherAddress nextHop;
      //GPSPosition *nhpos;

      /*nhpos =*/ _rt->getClosestNeighbour(&dpos, &nextHop);

      WritablePacket *out_packet = BRNProtocol::add_brn_header(packet, BRN_PORT_GEOROUTING, BRN_PORT_GEOROUTING, ttl);
      BRNPacketAnno::set_ether_anno(out_packet, *_nodeid->getMasterAddress(), nextHop, ETHERTYPE_BRN);

      output(0).push(out_packet);
    } else {
      GeorProtocol::stripRoutingHeader(packet);

      if ( packet->push(12) ) {
        if ( BRNProtocol::is_brn_etherframe(packet) ) { //set ttl if payload is brn_packet
          struct click_brn *brnh = BRNProtocol::get_brnheader_in_etherframe(packet);
          brnh->ttl = ttl;
        }
        packet->pull(12);
      }
      uint16_t *ethertype = (uint16_t*)(packet->data());
      BRNPacketAnno::set_ether_anno(packet, sea, dea, ntohs(*ethertype));
      packet->pull(sizeof(uint16_t));

      output(1).push(packet);
    }
  } else {
    packet->kill();
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  GeorForwarder *gf = (GeorForwarder *)e;
  return String(gf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  GeorForwarder *gf = (GeorForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  gf->_debug = debug;
  return 0;
}

void
GeorForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GeorForwarder)
