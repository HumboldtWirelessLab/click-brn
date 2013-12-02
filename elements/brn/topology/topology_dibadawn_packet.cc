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


CLICK_DECLS


#pragma pack(1)
struct DibadawnPacketStruct
{
  uint8_t version : 4;
  uint8_t reserve : 2;
  uint8_t type : 2;

  uint8_t id[DibadawnSearchId::length];
  uint8_t forwaredBy[6];
  uint8_t treeParent[6];
  uint8_t ttl;
};

DibadawnPacket::DibadawnPacket()
{
  setVersion();
}

DibadawnPacket::DibadawnPacket(const Packet *brn_packet)
{
  DibadawnPacketStruct *packet;
  packet = (struct DibadawnPacketStruct *) brn_packet->data();
  
  forwardedBy = EtherAddress(packet->forwaredBy);
  treeParent = EtherAddress(packet->treeParent);
  ttl = packet->ttl;
  isForward = (packet->type & 0x03) != 0;
  searchId.setByPointerTo10BytesOfData(packet->id);
  version = packet->version;
}

DibadawnPacket::DibadawnPacket(DibadawnSearchId *id, const EtherAddress &sender_addr, bool is_forward)
{
  setVersion();
  searchId = *id;
  forwardedBy = sender_addr;
  isForward = is_forward;
  ttl = 255;
}

bool DibadawnPacket::isValid(const Packet *brn_packet)
{
  DibadawnPacketStruct *packet;
  packet = (struct DibadawnPacketStruct *) brn_packet->data();
  return (packet->version == 1);
}

void DibadawnPacket::setVersion()
{
  this->version = 1;
}

void DibadawnPacket::setForwaredBy(const EtherAddress* sender_addr)
{
  forwardedBy = *sender_addr;
}

void DibadawnPacket::setTreeParent(const EtherAddress* sender_addr)
{
  treeParent = *sender_addr;
}

WritablePacket* DibadawnPacket::getBrnPacket()
{
  DibadawnPacketStruct content;
  content.version = version & 0x0F;
  content.type = isForward ? 1 : 0;
  memcpy(content.id, searchId.PointerTo10BytesOfData(), sizeof (content.id));
  memcpy(content.forwaredBy, forwardedBy.data(), sizeof (content.forwaredBy));
  memcpy(content.treeParent, treeParent.data(), sizeof (content.treeParent));
  content.ttl = ttl; 

  uint32_t sizeof_head = 128;
  uint32_t sizeof_tail = 32;
  WritablePacket *packet = WritablePacket::make(
      sizeof_head,
      &content, 
      sizeof(content),
      sizeof_tail);

  WritablePacket *brn_packet = BRNProtocol::add_brn_header(
      packet,
      BRN_PORT_TOPOLOGY_DETECTION,
      BRN_PORT_TOPOLOGY_DETECTION,
      128,
      0);

  BRNPacketAnno::set_ether_anno(
      brn_packet,
      forwardedBy.data(),
      brn_ethernet_broadcast,
      ETHERTYPE_BRN);

  return (brn_packet);
}

void DibadawnPacket::log(String tag, EtherAddress &thisNode)
{
  String forwaredByAsText = forwardedBy.unparse_dash();
  String treeParrentAsText = treeParent.unparse_dash();
  String thisNodeAsText = thisNode.unparse_dash();
  
  click_chatter("\n<%s node='%s' version='%d' type='%d' ttl='%d' searchId='%s' forwardedBy='%s' treeParent='%s' />", 
      tag.c_str(),
      thisNodeAsText.c_str(),
      version, 
      isForward,
      ttl,
      searchId.AsString().c_str(),
      forwaredByAsText.c_str(),
      treeParrentAsText.c_str());
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnPacket)
