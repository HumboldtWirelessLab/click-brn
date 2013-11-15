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
#include <click/config.h>
#include <click/etheraddress.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "topology_detection_protocol.hh"

CLICK_DECLS

WritablePacket *
TopologyDetectionProtocol::new_detection_packet(const EtherAddress *src, uint32_t id, uint8_t ttl)
{
  struct td_header *tdh;
  WritablePacket *td_packet = WritablePacket::make( 128, NULL, sizeof(struct td_header), 32);

  tdh = (struct td_header *)td_packet->data();

  tdh->id = htonl(id);
  memcpy(tdh->src,src->data(),6);
  tdh->entries = 0;
  tdh->ttl = ttl;
  tdh->type = 3;

  WritablePacket *brn_p = BRNProtocol::add_brn_header(td_packet, BRN_PORT_TOPOLOGY_DETECTION,
                                                                 BRN_PORT_TOPOLOGY_DETECTION, 128, 0);

  BRNPacketAnno::set_ether_anno(brn_p, src->data(), brn_ethernet_broadcast, ETHERTYPE_BRN);

  return brn_p;
}

WritablePacket *
TopologyDetectionProtocol::fwd_packet(Packet *p, const EtherAddress *src, EtherAddress *new_node)
{
  struct td_header *tdh;

  uint32_t oldlen = p->length();
  WritablePacket *td_packet = p->put(6); //space for one additional etheraddress

  tdh = (struct td_header *)td_packet->data();
  tdh->entries++;
  tdh->ttl--;

  memcpy((uint8_t*)&(td_packet->data()[oldlen]), new_node->data(),6);

  WritablePacket *brn_p = BRNProtocol::add_brn_header(td_packet, BRN_PORT_TOPOLOGY_DETECTION,
                                                                 BRN_PORT_TOPOLOGY_DETECTION, 128, 0);

  BRNPacketAnno::set_ether_anno(brn_p, src->data(), brn_ethernet_broadcast, ETHERTYPE_BRN);

  return brn_p;
}

uint8_t*
TopologyDetectionProtocol::get_info(Packet *p, EtherAddress *src, uint32_t *id, uint8_t *n_entries, uint8_t *ttl, uint8_t *type)
{
  struct td_header *tdh;

  tdh = (struct td_header *)p->data();
  *id = ntohl(tdh->id);
  *n_entries = tdh->entries;
  *src = EtherAddress(tdh->src);
  *ttl = tdh->ttl;
  *type = tdh->type;

  return (uint8_t*)&tdh[1];
}

WritablePacket *
TopologyDetectionProtocol::new_backwd_packet(EtherAddress *td_src, uint32_t td_id, const EtherAddress *src, EtherAddress *dst, Vector<TopologyInfo::Bridge> */*brigdes*/)
{
  struct td_header *tdh;
  WritablePacket *td_packet = WritablePacket::make( 128, NULL, sizeof(struct td_header), 32);

  tdh = (struct td_header *)td_packet->data();

  tdh->id = htonl(td_id);
  memcpy(tdh->src, td_src->data(),6);
  tdh->entries = 0;
  tdh->ttl = 0;

  WritablePacket *brn_p = BRNProtocol::add_brn_header(td_packet, BRN_PORT_TOPOLOGY_DETECTION, BRN_PORT_TOPOLOGY_DETECTION, 128, 0);

  BRNPacketAnno::set_ether_anno(brn_p, src->data(), dst->data(), ETHERTYPE_BRN);

  return td_packet;
}

void
TopologyDetectionProtocol::get_info_backwd_packet(Packet */*p*/, Vector<TopologyInfo::Bridge> */*brigdes*/)
{

}

CLICK_ENDDECLS
ELEMENT_PROVIDES(TopologyDetectionProtocol)
