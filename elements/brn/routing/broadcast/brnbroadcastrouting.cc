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
 * BrnBroadcastRouting.{cc,hh}
 */

#include <click/config.h>

#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brnbroadcastrouting.hh"

CLICK_DECLS

BrnBroadcastRouting::BrnBroadcastRouting()
{
  BRNElement::init();
}

BrnBroadcastRouting::~BrnBroadcastRouting()
{
}

int
BrnBroadcastRouting::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_node_id,         //Use this for srcaddr
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BrnBroadcastRouting::initialize(ErrorHandler *)
{
  return 0;
}

void
BrnBroadcastRouting::push( int port, Packet *packet )
{
  BRN_DEBUG("BrnBroadcastRouting: PUSH :%s\n",_node_id->getMasterAddress()->unparse().c_str());

  click_ether *ether;

  if ( port == 0 )  //from client
  {
    BRN_DEBUG("BrnBroadcastRouting: PUSH vom Client :%s\n",_node_id->getMasterAddress()->unparse().c_str());

    uint8_t ttl = BRNPacketAnno::ttl_anno(packet);
    if ( ttl == 0 ) ttl = BROADCASTROUTING_DAFAULT_MAX_HOP_COUNT;

    ether = (click_ether *)packet->data();
    EtherAddress src = EtherAddress(ether->ether_shost);

    WritablePacket *out_packet = BRNProtocol::add_brn_header(packet, BRN_PORT_BCASTROUTING, BRN_PORT_BCASTROUTING, ttl);
    BRNPacketAnno::set_ether_anno(out_packet, src.data(), brn_ethernet_broadcast, ETHERTYPE_BRN);
    output(1).push(out_packet);  //to brn -> flooding
  }

  if ( port == 1 )               // from brn (flooding)
  {
    BRN_DEBUG("BrnBroadcastRouting: PUSH von BRN :%s\n",_node_id->getMasterAddress()->unparse().c_str());

    uint8_t *packet_data = (uint8_t *)packet->data();
    ether = (click_ether *)packet_data;
    EtherAddress dst_addr = EtherAddress(ether->ether_dhost);

    if ( _node_id->isIdentical(&dst_addr) || dst_addr.is_broadcast() ) {
      BRN_DEBUG("This is for me");
      ether = (click_ether *)packet->data();
      packet->set_ether_header(ether);

      uint8_t ttl = BRNPacketAnno::ttl_anno(packet);
      if ( BRNProtocol::is_brn_etherframe(packet) )
        BRNProtocol::get_brnheader_in_etherframe(packet)->ttl = ttl;

      output(0).push(packet);
    } else {
      BRN_DEBUG("Not for me");
      packet->kill();
    }
  }

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BrnBroadcastRouting::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnBroadcastRouting)
