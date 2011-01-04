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
 * flooding.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>


#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "flooding.hh"
#include "floodingpolicy/floodingpolicy.hh"

CLICK_DECLS

Flooding::Flooding()
  : _flooding_src(0),
    _flooding_fwd(0)
{
  BRNElement::init();
}

Flooding::~Flooding()
{
  bcast_map.clear();
}

int
Flooding::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FLOODINGPOLICY", cpkP+cpkM , cpElement, &_flooding_policy,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
Flooding::initialize(ErrorHandler *)
{
  bcast_id = 0;
  return 0;
}

void
Flooding::push( int port, Packet *packet )
{
  BRN_DEBUG("Flooding: PUSH\n");

  struct click_brn_bcast *bcast_header;
  EtherAddress src;

  uint8_t ttl = BRNPacketAnno::ttl_anno(packet);

  if ( port == 0 )                       // kommt von Client (arp oder so)
  {
    Timestamp now = Timestamp::now();
    BRN_DEBUG("Flooding: PUSH vom Client\n");

    click_ether *ether = (click_ether *)packet->data();
    src = EtherAddress(ether->ether_shost);

    packet->pull(6);                                                           //remove mac Broadcast-Address
    WritablePacket *new_packet = packet->push(sizeof(struct click_brn_bcast)); //add BroadcastHeader
    struct click_brn_bcast *bch = (struct click_brn_bcast*)new_packet->data();

    bch->bcast_id = htons(++bcast_id);

    add_id(&src,(int32_t)bcast_id, &now);

    _flooding_policy->add_broadcast(&src,(int)bcast_id);
    _flooding_src++;                                                           //i was src of a flooding

    if ( ttl == 0 ) ttl = DEFAULT_TTL;

    WritablePacket *out_packet = BRNProtocol::add_brn_header(new_packet, BRN_PORT_FLOODING, BRN_PORT_FLOODING,
                                                                         ttl, DEFAULT_TOS);
    BRNPacketAnno::set_ether_anno(out_packet, brn_ethernet_broadcast, brn_ethernet_broadcast, ETHERTYPE_BRN);

    BRN_DEBUG("New Broadcast from %s. ID: %d",src.unparse().c_str(),bcast_id);

    output(1).push(out_packet);

  } else if ( port == 1 ) {                                   // kommt von brn

    BRN_DEBUG("Flooding: PUSH von BRN\n");

    Timestamp now = packet->timestamp_anno();

    bcast_header = (struct click_brn_bcast *)(packet->data());
    src = EtherAddress((uint8_t*)&(packet->data()[sizeof(struct click_brn_bcast)]));

    uint16_t p_bcast_id = ntohs(bcast_header->bcast_id);
    bool is_known = have_id(&src, p_bcast_id, &now);
    ttl--;

    bool forward = _flooding_policy->do_forward(&src, p_bcast_id, is_known);

    if ( ! is_known ) {   //note and send to client only if this is the first time
      Packet *p_client;

      add_id(&src,(int32_t)p_bcast_id, &now);

      if ( forward )
        p_client = packet->clone();                           //packet for client
      else
        p_client = packet;

      p_client->pull(sizeof(struct click_brn_bcast));         //remove bcast_header
      WritablePacket *p_client_out = p_client->push(6);       //add space for bcast_addr
      memcpy(p_client_out->data(), brn_ethernet_broadcast, 6);//set dest to bcast

      if ( BRNProtocol::is_brn_etherframe(p_client) )
        BRNProtocol::get_brnheader_in_etherframe(p_client)->ttl = ttl;

      output(0).push(p_client_out);                           // to clients (arp,...)
    }

    if (forward)
    {
      _flooding_fwd++;

      BRN_DEBUG("Forward: %s ID:%d", src.unparse().c_str(), p_bcast_id);

      WritablePacket *out_packet = BRNProtocol::add_brn_header(packet, BRN_PORT_FLOODING, BRN_PORT_FLOODING, ttl, DEFAULT_TOS);
      BRNPacketAnno::set_ether_anno(out_packet, brn_ethernet_broadcast, brn_ethernet_broadcast, ETHERTYPE_BRN);

      output(1).push(out_packet);

    } else {
      BRN_DEBUG("No forward: %s:%d",src.unparse().c_str(), p_bcast_id);
      if ( is_known ) packet->kill();  //no forwarding and already known (no forward to client) , so kill it
    }
  }

}

void
Flooding::add_id(EtherAddress *src, int32_t id, Timestamp *now)
{
  BroadcastNode *bcn = bcast_map.findp(*src);

  if ( bcn == NULL ) bcast_map.insert(*src, BroadcastNode(src, id));
  else bcn->add_id(id,*now);
}

bool
Flooding::have_id(EtherAddress *src, int32_t id, Timestamp *now)
{
  BroadcastNode *bcn = bcast_map.findp(*src);

  if ( bcn == NULL ) return false;

  return bcn->have_id(id,*now);
}

void
Flooding::reset()
{
  _flooding_src = _flooding_fwd = 0;
}


String
Flooding::stats()
{
  StringAccum sa;

  sa << "<flooding node=\"" << BRN_NODE_NAME << "\" >\n\t<source count=\"" << _flooding_src << "\" />\n";
  sa << "\t<forward count=\"" << _flooding_fwd << "\" />\n</flooding>\n";

  return sa.take_string();
}
//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_stats_param(Element *e, void *)
{
  return ((Flooding *)e)->stats();
}

static int 
write_reset_param(const String &/*in_s*/, Element *e, void */*vparam*/, ErrorHandler */*errh*/)
{
  Flooding *fl = (Flooding *)e;
  fl->reset();

  return 0;
}


void
Flooding::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_stats_param, 0);

  add_write_handler("reset", write_reset_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Flooding)
