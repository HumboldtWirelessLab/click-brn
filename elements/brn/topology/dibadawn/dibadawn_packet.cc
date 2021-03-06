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

#define HEADER_SIZE 30

CLICK_DECLS


DibadawnPacket::DibadawnPacket()
 : isForward(false), hops(0), createdByInvalidPacket(false)
{
  setVersion();
  sumForwardDelay = 0;
  lastForwardDelayMs = 0;
}

DibadawnPacket::DibadawnPacket(Packet &brn_packet)
:createdByInvalidPacket(false){
  if (!isValid(brn_packet))
  {
    createdByInvalidPacket = true;
    return;
  }
  createdByInvalidPacket = false;

  size_t n = deserialzeData(brn_packet.data());
  assert(n >= HEADER_SIZE);
}

size_t DibadawnPacket::deserialzeData(const uint8_t *src)
{
  size_t offset = 0;  
  
  memcpy(&version, src + offset, sizeof(version));
  offset += sizeof(version);
  
  uint8_t netIsForward;
  memcpy(&netIsForward, src + offset, sizeof(netIsForward));
  isForward = netIsForward != 0;
  offset += sizeof(netIsForward);

  searchId.setByPointerTo10BytesOfData(src + offset);
  offset += searchId.length;
  
  forwardedBy = EtherAddress(src + offset);
  offset += 6;
  
  treeParent = EtherAddress(src + offset);
  offset += 6;
  
  memcpy(&hops, src + offset, sizeof(hops));
  offset += sizeof(hops);
  
  uint16_t netSumForwardDelay;
  memcpy(&netSumForwardDelay, src + offset, sizeof(netSumForwardDelay));
  sumForwardDelay = ntohs(netSumForwardDelay);
  offset += sizeof(netSumForwardDelay);
  
  uint16_t netLastForwardDelayMs;
  memcpy(&netLastForwardDelayMs, src + offset, sizeof(netLastForwardDelayMs));
  lastForwardDelayMs = ntohs(netLastForwardDelayMs);
  offset += sizeof(netLastForwardDelayMs);
  
  uint8_t numPayloads;
  memcpy(&numPayloads, src + offset, sizeof(numPayloads));
  offset += sizeof(numPayloads);
  
  assert(HEADER_SIZE == offset);
  
  for (int i = 0; i < numPayloads; i++)
  {
    payload.push_back(DibadawnPayloadElement(src + offset));
    offset += DibadawnPayloadElement::length;
  }
  
  return(offset);
}

DibadawnPacket::DibadawnPacket(DibadawnSearchId &id, EtherAddress &sender_addr, bool is_forward): createdByInvalidPacket(false)
{
  setVersion();
  searchId = id;
  forwardedBy = sender_addr;
  isForward = is_forward;
  hops = 0;
  lastForwardDelayMs = 0;
  sumForwardDelay = 0;
}

bool DibadawnPacket::isValid(Packet &brn_packet)
{
  uint8_t* version = (uint8_t*) brn_packet.data();
  return (*version == 1);
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
  size_t dibadawnPacketSize = HEADER_SIZE + payload.size() * DibadawnPayloadElement::length;
  uint8_t* dibadawnPacket = (uint8_t*) malloc(dibadawnPacketSize);
  if(dibadawnPacket == NULL)
  {
    click_chatter("<DibadawnError where='DibadawnPacket::getBrnPacket' reason='malloc failed' />");
    return(NULL);
  }
  
  serialzeData(dibadawnPacket);

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

size_t DibadawnPacket::serialzeData(uint8_t* dest)
{
  size_t offset = 0;  
  
  memcpy(dest + offset, &version, sizeof(version));
  offset += sizeof(version);
  
  uint8_t netIsForward = isForward? 1: 0;
  memcpy(dest + offset, &netIsForward, sizeof(netIsForward));
  offset += sizeof(netIsForward);

  memcpy(dest + offset, searchId.PointerTo10BytesOfData(), searchId.length);
  offset += searchId.length;
  
  memcpy(dest + offset, forwardedBy.data(), 6);
  offset += 6;
  
  memcpy(dest + offset, treeParent.data(), 6);
  offset += 6;
  
  memcpy(dest + offset, &hops, sizeof(hops));
  offset += sizeof(hops);
  
  uint16_t netSumForwardDelay = htons(sumForwardDelay);
  memcpy(dest + offset, &netSumForwardDelay, sizeof(netSumForwardDelay));
  offset += sizeof(netSumForwardDelay);
  
  uint16_t netLastForwardDelayMs = htons(lastForwardDelayMs);
  memcpy(dest + offset, &netLastForwardDelayMs, sizeof(netLastForwardDelayMs));
  offset += sizeof(netLastForwardDelayMs);
  
  uint8_t netSize = (uint8_t)payload.size();
  memcpy(dest + offset, &netSize, sizeof(netSize));
  offset += sizeof(netSize);
  
  assert(HEADER_SIZE == offset);
  
  for (int i = 0; i < payload.size(); i++)
  {
    memcpy(dest + offset, payload.at(i).getData(), DibadawnPayloadElement::length);
    offset += DibadawnPayloadElement::length;
  }
  
  return(offset);
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

void DibadawnPacket::log(String tag, EtherAddress &thisNode, String parent_src_attr)
{
  StringAccum sa;
  sa << "<" << tag << " ";
  sa << "node='" << thisNode.unparse_dash() << "' ";
  sa << parent_src_attr << " ";
  sa << "time='" << Timestamp::now().unparse() << "' ";
  sa << "searchId='" << searchId << "' ";
  sa << "forwardedBy='" << forwardedBy.unparse_dash() << "' ";
  sa << "version='" << (int)version << "' ";
  sa << "type='" << isForward << "' ";
  sa << "type_descr='" << (isForward ? "ForwardMsg" : "BackMsg") << "' ";
  sa << "hop='" << int(hops) << "' ";
  sa << "treeParent='" << treeParent.unparse_dash() << "' ";
  sa << "sumForwardDelay='" << sumForwardDelay << "' ";
  sa << "lastForwardDelay='" << lastForwardDelayMs << "' ";
  sa << "numPayload='" << payload.size() << "' ";
  
  if (payload.size() > 0)
  {
    sa << ">\n";
    sa << "  <Payload>\n";
    for (int i = 0; i < payload.size(); i++)
    {
      DibadawnPayloadElement &elem = payload.at(i);
      sa << "    " << elem << "\n";
    }
    sa << "  </Payload>\n";
    sa << "</" << tag << ">";
  }
  else
  {
    sa << "/>";
  }

  click_chatter(sa.take_string().c_str());
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
  for (Vector<DibadawnPayloadElement>::iterator it = payload.begin(); it != payload.end(); ++it)
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
  for (Vector<DibadawnPayloadElement>::iterator it = payload.begin(); it != payload.end(); ++it)
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
