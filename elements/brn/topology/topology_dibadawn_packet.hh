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

/* Sender-/Receivernumbers */
/* field: sender,receiver */
#ifndef TOPOLOGY_DIBADAWN_PACKET_HH
#define TOPOLOGY_DIBADAWN_PACKET_HH

#include <click/element.hh>
#include <click/packet.hh>

#include "topology_dibadawn_searchid.hh"

CLICK_DECLS;

class DibadawnPacket {
public:
  DibadawnPacket();
  DibadawnPacket(const Packet *packet);
  DibadawnPacket(DibadawnSearchId *id, const EtherAddress* sender_addr, bool is_forward);
  
  void setVersion();
  void setTreeParent(const EtherAddress* sender_addr);
  void setForwaredBy(const EtherAddress* sender_addr);
  WritablePacket* getBrnPacket();
  static bool isValid(const Packet *packet);
  
  uint32_t version; 
  DibadawnSearchId searchId;
  static const size_t LengthOfAddr = 6;
  uint8_t treeParrent[LengthOfAddr];
  uint8_t forwardedBy[LengthOfAddr];
  bool isForward;
  uint8_t ttl;
  
private:

};

CLICK_ENDDECLS
#endif
