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
      "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_my_ether_addr,  //TODO: replace by nodeid and send paket to each (wireless) device
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BrnBroadcastRouting::initialize(ErrorHandler *)
{
  bcast_id = 0;

  return 0;
}

void
BrnBroadcastRouting::push( int port, Packet *packet )
{
  click_chatter("BrnBroadcastRouting: PUSH\n");
  struct click_bcast_routing_header *bc_header;  /*uint16_t  bcast_id*/
  click_ether *ether;

  uint8_t broadcast[] = { 255,255,255,255,255,255 };

  if ( port == 0 )  //from client
  {
    click_chatter("BrnBroadcastRouting: PUSH vom Client\n");
    ether = (click_ether *)packet->data();
    bcast_id++;
    bcast_queue.push_back(BrnBroadcast( bcast_id,ether->ether_shost,ether->ether_dhost));
    if ( bcast_queue.size() > MAX_QUEUE_SIZE ) bcast_queue.erase( bcast_queue.begin() );

    WritablePacket *out_packet = packet->push( sizeof(struct click_bcast_routing_header) );  //add id
    bc_header = (struct click_bcast_routing_header *)out_packet->data();
    bc_header->bcast_id = htons(bcast_id);

    WritablePacket *final_out_packet = BRNProtocol::add_brn_header(out_packet, BRN_PORT_BCAST, BRN_PORT_BCAST);
    BRNPacketAnno::set_ether_anno(final_out_packet, _my_ether_addr,EtherAddress(broadcast), 0x8680);

    output( 1 ).push( final_out_packet );  //to brn
  }

  if ( port == 1 )  // from brn
  {
    click_chatter("BrnBroadcastRouting: PUSH von BRN\n");

    uint8_t *packet_data = (uint8_t *)packet->data();
    struct click_bcast_routing_header *bc_header = (struct click_bcast_routing_header*)packet_data;
    ether = (click_ether *)&packet_data[sizeof(struct click_bcast_routing_header)];

    int i;
    for( i = 0; i < bcast_queue.size(); i++ )
     if ( ( bcast_queue[i].bcast_id == bc_header->bcast_id ) &&
          ( memcmp( &bcast_queue[i]._src[0], &ether->ether_shost, 6 ) == 0 ) &&
          ( memcmp( &bcast_queue[i]._dst[0], &ether->ether_dhost, 6 ) == 0 ) ) break;

    if ( i == bcast_queue.size() ) // paket noch nie gesehen
    {
      click_chatter("Queue size:%d Hab noch nie gesehen", bcast_queue.size());

      bcast_queue.push_back(BrnBroadcast(bc_header->bcast_id, ether->ether_shost, ether->ether_dhost));
      if ( bcast_queue.size() > MAX_QUEUE_SIZE ) bcast_queue.erase( bcast_queue.begin() );

      /* Packete an den Client sofort raus , an andere Knoten in die Jitter-Queue */
      Packet *p_client = packet->clone();

      p_client->pull(sizeof(struct click_bcast_routing_header));
      output( 0 ).push( p_client );  // to clients (arp,...)

      BRNPacketAnno::set_ether_anno(packet, _my_ether_addr, EtherAddress(broadcast), 0x8680);
      output( 1 ).push(packet);
    }
    else
      packet->kill();
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

#include <click/vector.cc>
template class Vector<BrnBroadcastRouting::BrnBroadcast>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnBroadcastRouting)
