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
#include <click/string.hh>
#include <click/glue.hh>
#include <click/type_traits.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "topology_dibadawn_packet.hh"
#include "topology_dibadawn_searchid.hh"

#define LOG BrnLogger(__FILE__, __LINE__, NULL).info

CLICK_DECLS


#pragma pack(1)
struct dibadawn_packet
{
  uint8_t version : 4;
  uint8_t reserve : 2;
  uint8_t type : 2;

  uint8_t id[DibadawnSearchId::length];
  uint8_t src[DibadawnPacket::LengthOfForwardedBy];
  uint8_t ttl;
};

DibadawnPacket::DibadawnPacket()
{
  setVersion();
}

DibadawnPacket::DibadawnPacket(const Packet *packet)
{
}

DibadawnPacket::DibadawnPacket(DibadawnSearchId *id, const EtherAddress* sender_addr, bool is_forward)
{
  setVersion();
  setSearchId(id);
  setTreeParent(NULL);
  setForwaredBy(sender_addr);
  isForward = is_forward;
}

bool DibadawnPacket::isValid(const Packet *brn_packet)
{
  struct dibadawn_packet *packet;
  packet = (struct dibadawn_packet *) brn_packet->data();
  return (packet->version == 1);
}

void DibadawnPacket::setVersion()
{
  this->version = 1;
}

void DibadawnPacket::setSearchId(DibadawnSearchId* id)
{
  this->searchId = *id;
}

void DibadawnPacket::setForwaredBy(const EtherAddress* sender_addr)
{
  const uint8_t *bytes = sender_addr->data();
  memcpy(forwardedBy, bytes, sizeof (forwardedBy));
}

void DibadawnPacket::setTreeParent(const EtherAddress* sender_addr)
{
  if (sender_addr == NULL)
  {
    memset(treeParrent, 0x00, sizeof (treeParrent));
  }
  else
  {
    const uint8_t *bytes = sender_addr->data();
    memcpy(treeParrent, bytes, sizeof (treeParrent));
  }
}

WritablePacket* DibadawnPacket::getBrnPacket()
{
  dibadawn_packet content;
  content.version = version & 0x0F;
  content.type = isForward ? 0 : 1;
  memcpy(content.id, searchId.PointerTo10BytesOfData(), sizeof (content.id));
  memcpy(content.src, forwardedBy, sizeof (content.src));
  content.ttl = 100; // TODO: Does this makes sense?

  uint32_t sizeof_head = 128;
  uint32_t sizeof_data = sizeof (content);
  uint32_t sizeof_tail = 32;
  WritablePacket *packet = WritablePacket::make(
      sizeof_head,
      &content, sizeof_data,
      sizeof_tail);

  WritablePacket *brn_packet = BRNProtocol::add_brn_header(
      packet,
      BRN_PORT_TOPOLOGY_DETECTION,
      BRN_PORT_TOPOLOGY_DETECTION,
      128,
      0);

  BRNPacketAnno::set_ether_anno(
      brn_packet,
      forwardedBy,
      brn_ethernet_broadcast,
      ETHERTYPE_BRN);

  return (brn_packet);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel | ns)
ELEMENT_PROVIDES(DibadawnPacket)
