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

#ifndef TOPOLOGY_DIBADAWN_PACKET_PAYLOADELEMET_HH
#define TOPOLOGY_DIBADAWN_PACKET_PAYLOADELEMET_HH

#include <click/element.hh>
#include <click/packet.hh>

#include "searchid.hh"
#include "cycle.hh"

CLICK_DECLS;

class DibadawnPayloadElement
{
public:
    bool isBridge;
    uint8_t hops;
    static const size_t length = DibadawnCycle::length + 1/* one byte for isBrigde */ + 1 /*sizeof(DibadawnPayloadElement::hops)*/;
    DibadawnCycle cycle;

    DibadawnPayloadElement(DibadawnSearchId &id, EtherAddress &nodeA, EtherAddress &nodeB, bool isBridge, uint8_t hops = 1);
    DibadawnPayloadElement(const DibadawnPayloadElement &dple);
    explicit DibadawnPayloadElement(const DibadawnCycle &cycle);
    explicit DibadawnPayloadElement(const uint8_t *pBinaryData);

    ~DibadawnPayloadElement() {
      if ( mayInconsistentlyData != NULL ) {
        delete[] mayInconsistentlyData;
        mayInconsistentlyData = NULL;
      }
    }

    void setCycle(DibadawnCycle& cycle);
    uint8_t* getData();
    bool operator==(DibadawnPayloadElement &b);

private:
    uint8_t *mayInconsistentlyData;
};

StringAccum& operator << (StringAccum &output, const DibadawnPayloadElement &id);


CLICK_ENDDECLS
#endif
