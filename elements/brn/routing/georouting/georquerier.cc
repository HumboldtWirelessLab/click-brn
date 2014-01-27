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
 * GeorQuerier.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"

#include "georquerier.hh"
#include "geortable.hh"
#include "georprotocol.hh"

CLICK_DECLS

GeorQuerier::GeorQuerier()
{
  BRNElement::init();
}

GeorQuerier::~GeorQuerier()
{
}

int
GeorQuerier::configure(Vector<String> &conf, ErrorHandler* errh)
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
GeorQuerier::initialize(ErrorHandler *)
{
  return 0;
}

void
GeorQuerier::push( int port, Packet *packet )
{
  uint8_t *data;
  if ( port == 0 ) {                               //Etherpacket to forward
    data = (uint8_t*)(packet->data());
    EtherAddress dea = EtherAddress(data);        //get destination
    EtherAddress sea = EtherAddress(&data[6]);    //get source

    BRN_INFO("Routequerier: %s to %s",sea.unparse().c_str(),dea.unparse().c_str());

    GPSPosition *dpos = _rt->getPosition(&dea);
    GPSPosition *spos = _rt->getLocalPosition();

    EtherAddress nextHop;
    GPSPosition *nhpos;

    if ( dpos ) {
      BRN_INFO("Routequerier Position: %s to %s",spos->unparse_coord().c_str(),dpos->unparse_coord().c_str());

      nhpos = _rt->getClosestNeighbour(dpos, &nextHop);

      if (nhpos) {
        uint8_t ttl = BRNPacketAnno::ttl_anno(packet);
        if ( ttl == 0 ) ttl = GEOR_DAFAULT_MAX_HOP_COUNT;

        BRN_INFO("Set ttl %d",(int)ttl);

        packet->pull(12);                           //strip etheradress
        WritablePacket *pout = GeorProtocol::addRoutingHeader(packet,&sea, spos, &dea, dpos);

        WritablePacket *out_packet = BRNProtocol::add_brn_header(pout, BRN_PORT_GEOROUTING, BRN_PORT_GEOROUTING,
                                                                 ttl, DEFAULT_TOS);
        BRNPacketAnno::set_ether_anno(out_packet, sea, nextHop, ETHERTYPE_BRN);

        output(0).push(out_packet);
      } else {
        BRN_ERROR("No next hop. Discard packet.");
        packet->kill();
      }
    } else {
      BRN_ERROR("No Dst-GPS");
      BRN_INFO("Perform route request !");
      packet->kill();
    }
  } else if ( port == 1 ) {                      //routereply
      packet->kill();
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
  GeorQuerier *gf = (GeorQuerier *)e;
  return String(gf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  GeorQuerier *gf = (GeorQuerier *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  gf->_debug = debug;
  return 0;
}

void
GeorQuerier::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(GeorProtocol)
EXPORT_ELEMENT(GeorQuerier)
