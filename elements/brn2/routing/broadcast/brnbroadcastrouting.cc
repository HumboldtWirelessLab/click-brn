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
//#include "elements/brn/common.hh"

#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"

#include "brnbroadcastrouting.hh"

CLICK_DECLS

BrnBroadcastRouting::BrnBroadcastRouting()
  :_debug(/*BrnLogger::DEFAULT*/0)
{
}

BrnBroadcastRouting::~BrnBroadcastRouting()
{
}

int
BrnBroadcastRouting::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_node_id,         //Use this for srcaddr
      "SOURCEADDRESS", cpkP, cpEtherAddress, &_my_ether_addr,
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
  click_chatter("BrnBroadcastRouting: PUSH :%s\n",_my_ether_addr.unparse().c_str());

  click_ether *ether;
  uint8_t broadcast[] = { 255,255,255,255,255,255 };

  if ( port == 0 )  //from client
  {
    click_chatter("BrnBroadcastRouting: PUSH vom Client :%s\n",_my_ether_addr.unparse().c_str());
    ether = (click_ether *)packet->data();
    EtherAddress src = EtherAddress(ether->ether_shost);

    WritablePacket *out_packet = BRNProtocol::add_brn_header(packet, BRN_PORT_BCASTROUTING, BRN_PORT_BCASTROUTING);
    BRNPacketAnno::set_ether_anno(out_packet, src, EtherAddress(broadcast), 0x8086);
    output(1).push(out_packet);  //to brn -> flooding
  }

  if ( port == 1 )  // from brn (flooding)
  {
    click_chatter("BrnBroadcastRouting: PUSH von BRN :%s\n",_my_ether_addr.unparse().c_str());

    uint8_t *packet_data = (uint8_t *)packet->data();
    ether = (click_ether *)packet_data;
    EtherAddress dst_addr = EtherAddress(ether->ether_dhost);

    if ( _node_id->isIdentical(&dst_addr) ) {
      click_chatter("This is for me");
      ether = (click_ether *)packet->data();
      packet->set_ether_header(ether);
      output(0).push(packet);
    } else {
      click_chatter("Not for me");
      packet->set_ether_header((click_ether*)packet_data);
      packet->kill();
    }
  }

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BrnBroadcastRouting *fl = (BrnBroadcastRouting *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BrnBroadcastRouting *fl = (BrnBroadcastRouting *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
BrnBroadcastRouting::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnBroadcastRouting)
