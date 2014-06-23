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

#ifndef TOPOLOGY_DIBADAWN_SEARCH_HH
#define TOPOLOGY_DIBADAWN_SEARCH_HH

#include <click/element.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "searchid.hh"
#include "dibadawn_packet.hh"
#include "payloadelement.hh"
#include "edgemarking.hh"
#include "cycle.hh"
#include "neighbor_container.hh"
#include "nodestatistic.hh"
#include "config.hh"


CLICK_DECLS;

class DibadawnSearch
{
public:

    struct ForwardSendTimerParam
    {
        DibadawnSearch *search;
        DibadawnPacket packet;
    };

private:
    EtherAddress parentNode;
    bool visited;
    Timer *forwardTimeoutTimer;
    Timer *forwardSendTimer;
    DibadawnSearchId searchId;
    Vector<DibadawnPacket> crossEdges;
    DibadawnNodeStatistic &commonStatistic;
    Vector<DibadawnPayloadElement> messageBuffer;
    DibadawnNeighborContainer adjacents;
    DibadawnConfig &config;
    bool isArticulationPoint;
    DibadawnPacket sentForwardPacket;
    
    uint32_t numOfConcurrentSenders;
    BRNElement *brn_click_element;

    void initCommon(BRNElement *click_element);
    void activateForwardTimer(DibadawnPacket &packet);
    void activateForwardSendTimer(DibadawnPacket &packet);
    void receiveForwardMessage(DibadawnPacket &packet);
    bool isValidCrossEdge(DibadawnPacket &rxPacket);
    void receiveBackMessage(DibadawnPacket &packet);
    void detectCycles();
    void bufferBackwardMessage(DibadawnCycle &cycleId);
    void forwardMessages();
    void detectArticulationPoints();
    void voteForArticulaionPointsAndBridges();
    void updateAdjacent(DibadawnPacket &packet);
    DibadawnPayloadElement* tryToFindPair(DibadawnPayloadElement &payloadA);
    void removePayloadFromMessageBuffer(DibadawnPayloadElement &payloadA);
    void setNonBrigdeByPayload(DibadawnPayloadElement &payload);
    void setParentNull();
    bool isParentNull();
    void addBridgeEdgeMarking(EtherAddress &nodeA, EtherAddress &nodeB, double competence, const char *src);
    void pairCyclesIfPossible(DibadawnPacket &packet);
    void addPayloadElementsToMessagePuffer(DibadawnPacket &packet);
    uint16_t calcForwardDelay();
    uint16_t calcForwardDelayImproved(DibadawnPacket &packet);

public:
    DibadawnSearch(BRNElement *brn_click_element, DibadawnNodeStatistic &stat, DibadawnConfig &cfg);
    DibadawnSearch(BRNElement *brn_click_element, DibadawnNodeStatistic &stat, DibadawnConfig &cfg, DibadawnSearchId &packet);

    void sendBroadcastWithTimeout(DibadawnPacket &packet);
    void sendTo(DibadawnPacket &packet, EtherAddress &dest);
    void sendDelayedBroadcastWithTimeout(DibadawnPacket &packet);
    void receive(DibadawnPacket &packet);
    void start_search();
    bool isResponsibleFor(DibadawnPacket &packet);
    void forwardTimeout();
};

CLICK_ENDDECLS
#endif
