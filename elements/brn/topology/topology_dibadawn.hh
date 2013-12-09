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

#ifndef TOPOLOGY_DIBADAWN_HH
#define TOPOLOGY_DIBADAWN_HH

#include <click/element.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "topology_dibadawn_searchid.hh"
#include "topology_info.hh"
#include "topology_dibadawn_packet.hh"
#include "topology_dibadawn_edgemarking.hh"
#include "topology_dibadawn_cycle.hh"


CLICK_DECLS;

class DibadawnSearch 
{
  EtherAddress thisNode;
  EtherAddress parent;
  DibadawnPacket outgoingPacket;
  bool visited;
  Timer *forwardTimer;
  DibadawnSearchId searchId;
  Vector<EtherAddress> crossEdges;
  Vector<DibadawnEdgeMarking> edgeMarkings;
  Vector<DibadawnPacket> messageBuffer;
  Vector<DibadawnPacket> msg;
  Vector<EtherAddress> adj;
  bool isArticulationPoint;
  
  uint32_t maxTraversalTimeMs;
  uint8_t maxTtl;
  BRNElement *brn_click_element;
  
  void sendPerBroadcastWithTimeout();
  void sendPerBroadcastWithoutTimeout();
  void initTimer();
  void activateForwardTimer();
  void receiveForwardMessage(DibadawnPacket &packet);
  void detectCycles();
  void bufferBackwardMessage(DibadawnCycle &cycleId);
  void forwardMessages();
  void detectAccessPoints();
  void voteForAccessPointsAndBridges();
  void AccessPointDetection();
  void initMatrixWithFalse(bool &m[][], const size_t l, const size_t c);
  
public:
  DibadawnSearch(BRNElement *brn_click_element, const EtherAddress &addrOfThisNode);
  DibadawnSearch(BRNElement *brn_click_element, const EtherAddress &addrOfThisNode, DibadawnPacket &packet);
  
  String asString();
  void receive(DibadawnPacket &packet);
  void start_search();
  bool isResponsableFor(DibadawnPacket &packet);
  void forwardTimeout(); 
};

CLICK_ENDDECLS
#endif
