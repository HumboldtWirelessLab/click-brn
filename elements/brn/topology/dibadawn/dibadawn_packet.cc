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
#include "dibadawn_packet.hh"
#include "searchid.hh"

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

  uint8_t numPayloads;
};

DibadawnPacket::DibadawnPacket()
{
  setVersion();
  createdByInvalidPacket = false;
}

DibadawnPacket::DibadawnPacket(Packet &brn_packet)
{
  if (!isValid(brn_packet))
  {
    createdByInvalidPacket = true;
    return;
  }
  createdByInvalidPacket = false;

  DibadawnPacketStruct *packet;
  packet = (struct DibadawnPacketStruct *) brn_packet.data();

  forwardedBy = EtherAddress(packet->forwaredBy);
  treeParent = EtherAddress(packet->treeParent);
  ttl = packet->ttl;
  isForward = (packet->type & 0x03) != 0;
  searchId.setByPointerTo10BytesOfData(packet->id);
  version = packet->version;

  const uint8_t* data = brn_packet.data() + sizeof (DibadawnPacketStruct);
  for (int i = 0; i < packet->numPayloads; i++)
  {
    payload.push_back(DibadawnPayloadElement(data));
    data += DibadawnPayloadElement::length;
  }
}

DibadawnPacket::DibadawnPacket(DibadawnSearchId &id, EtherAddress &sender_addr, bool is_forward)
{
  setVersion();
  searchId = id;
  forwardedBy = sender_addr;
  isForward = is_forward;
  ttl = 255;
}

bool DibadawnPacket::isValid(Packet &brn_packet)
{
  DibadawnPacketStruct *packet;
  packet = (struct DibadawnPacketStruct *) brn_packet.data();
  return (packet->version == 1);
}

bool DibadawnPacket::isInvalid()
{
  return (createdByInvalidPacket);
}

void DibadawnPacket::setVersion()
{
  this->version = 1;
}

void DibadawnPacket::setForwaredBy(EtherAddress* sender_addr)
{
  forwardedBy = *sender_addr;
}

void DibadawnPacket::setTreeParent(EtherAddress* sender_addr)
{
  treeParent = *sender_addr;
}

WritablePacket* DibadawnPacket::getBrnBroadcastPacket()
{
  EtherAddress a = getBroadcastAddress();
  return (getBrnPacket(a));
}

EtherAddress DibadawnPacket::getBroadcastAddress()
{
  return (EtherAddress(brn_ethernet_broadcast));
}

WritablePacket* DibadawnPacket::getBrnPacket(EtherAddress &dest)
{
  DibadawnPacketStruct content;
  content.version = version & 0x0F;
  content.type = isForward ? 1 : 0;
  memcpy(content.id, searchId.PointerTo10BytesOfData(), sizeof (content.id));
  memcpy(content.forwaredBy, forwardedBy.data(), sizeof (content.forwaredBy));
  memcpy(content.treeParent, treeParent.data(), sizeof (content.treeParent));
  content.ttl = ttl;
  content.numPayloads = payload.size();

  size_t dibadawnPacketSize = sizeof (content) + payload.size() * DibadawnPayloadElement::length;
  uint8_t* dibadawnPacket = (uint8_t*) malloc(dibadawnPacketSize);
  memcpy(dibadawnPacket, &content, sizeof (content));
  for (int i = 0; i < content.numPayloads; i++)
  {
    memcpy(dibadawnPacket + sizeof (content) + i * DibadawnPayloadElement::length,
        payload.at(i).getData(),
        DibadawnPayloadElement::length);
  }

  uint32_t sizeof_head = 128;
  uint32_t sizeof_tail = 32;
  WritablePacket *packet = WritablePacket::make(
      sizeof_head,
      dibadawnPacket,
      dibadawnPacketSize,
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
      dest,
      ETHERTYPE_BRN);

  free(dibadawnPacket);

  return (brn_packet);
}

void DibadawnPacket::logTx(EtherAddress &thisNode, EtherAddress to)
{
  String attr = "dest='" + to.unparse_dash() + "'";

  log("DibadawnPacketTx", thisNode, attr);
}

void DibadawnPacket::logRx(EtherAddress &thisNode)
{
  log("DibadawnPacketRx", thisNode, "");
}

void DibadawnPacket::log(String tag, EtherAddress &thisNode, String attr)
{
  String forwaredByAsText = forwardedBy.unparse_dash();
  String treeParrentAsText = treeParent.unparse_dash();
  String thisNodeAsText = thisNode.unparse_dash();

  click_chatter("<%s node='%s' %s >",
      tag.c_str(),
      thisNodeAsText.c_str(),
      attr.c_str());

  click_chatter("  <version>%d</version>", version);
  click_chatter("  <type>%d(%s)</type>", isForward, isForward ? "ForwardMsg" : "BackMsg");
  click_chatter("  <ttl>%d</ttl>", ttl);
  click_chatter("  <searchId>%s</searchId>", searchId.AsString().c_str());
  click_chatter("  <forwardedBy>%s</forwardedBy>", forwaredByAsText.c_str());
  click_chatter("  <treeParent>%s</treeParent>", treeParrentAsText.c_str());
  click_chatter("  <numPayload>%d</numPayload>", payload.size());

  if (payload.size() > 0)
  {
    click_chatter("  <Payload>");
    for (int i = 0; i < payload.size(); i++)
    {
      DibadawnPayloadElement &elem = payload.at(i);
      elem.print("    ");
    }
    click_chatter("  </Payload>");
  }

  click_chatter("</%s>", tag.c_str());
}

bool DibadawnPacket::hasSameCycle(DibadawnPacket& other)
{
  for (int i = 0; i < other.payload.size(); i++)
  {
    DibadawnPayloadElement& elem = other.payload.at(i);
    if (elem.isBridge)
      continue;

    hasSameCycle(elem);
  }
  return (false);
}

bool DibadawnPacket::hasSameCycle(DibadawnPayloadElement& elem)
{
  for (int i = 0; i < payload.size(); i++)
  {
    DibadawnPayloadElement& elem2 = payload.at(i);
    if (elem2.isBridge)
      continue;

    if (elem.cycle == elem2.cycle)
      return (true);
  }
  return (false);
}

void DibadawnPacket::removeCycle(DibadawnPayloadElement &elem)
{
  for (Vector<DibadawnPayloadElement>::iterator it = payload.begin(); it != payload.end(); it++)
  {
    DibadawnPayloadElement& elem2 = *it;
    if (elem2.isBridge)
      continue;

    if (elem.cycle == elem2.cycle)
    {
      payload.erase(it);
      return;
    }
  }
}

bool DibadawnPacket::hasBridgePayload()
{
  for (Vector<DibadawnPayloadElement>::iterator it = payload.begin(); it != payload.end(); it++)
  {
    DibadawnPayloadElement& elem = *it;
    if (elem.isBridge)
      return (true);
  }
  return (false);
}

void DibadawnPacket::copyPayloadIfNecessary(DibadawnPayloadElement& src)
{
  if (!hasSameCycle(src))
    payload.push_back(src);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnPacket)
