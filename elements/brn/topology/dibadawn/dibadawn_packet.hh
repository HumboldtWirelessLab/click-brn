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

#ifndef TOPOLOGY_DIBADAWN_PACKET_HH
#define TOPOLOGY_DIBADAWN_PACKET_HH

#include <click/element.hh>
#include <click/packet.hh>

#include "searchid.hh"
#include "cycle.hh"
#include "payloadelement.hh"

CLICK_DECLS;

class DibadawnPacket
{
public:
    DibadawnPacket();
    DibadawnPacket(Packet &packet);
    DibadawnPacket(DibadawnSearchId &id, EtherAddress &sender_addr, bool is_forward);

    void setVersion();
    void setTreeParent(EtherAddress *sender_addr);
    void setForwaredBy(EtherAddress *sender_addr);

    WritablePacket* getBrnPacket(EtherAddress &dest);
    WritablePacket* getBrnBroadcastPacket();
    EtherAddress getBroadcastAddress();

    static bool isValid(Packet &packet);
    bool isInvalid();

    void logTx(EtherAddress &thisNode, EtherAddress destination);
    void logRx(EtherAddress &thisNode);
    void log(String tag, EtherAddress &thisNode, String attr);

    bool hasSameCycle(DibadawnPacket &other);
    bool hasSameCycle(DibadawnPayloadElement &payload);
    void removeCycle(DibadawnPayloadElement &payload);
    bool hasBridgePayload();
    void copyPayloadIfNecessary(DibadawnPayloadElement &src);

    uint32_t version;
    DibadawnSearchId searchId;
    EtherAddress forwardedBy;
    EtherAddress treeParent;
    bool isForward;
    uint8_t ttl;
    bool createdByInvalidPacket;

    // Only used in backward messages.
    Vector<DibadawnPayloadElement> payload;

};

CLICK_ENDDECLS
#endif
