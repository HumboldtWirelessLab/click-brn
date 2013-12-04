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

CLICK_DECLS;

class DibadawnSearch 
{
  BRNElement *brn_click_element;
  EtherAddress thisNode;
  DibadawnPacket ideaOfPacket;
  bool firstProcessedPacket;
  Timer *forwardTimer;
  uint32_t maxTraversalTimeMs;
  bool inactive;
  Vector<EtherAddress> crossEdges;
  
  void sendPerBroadcastWithTimeout();
  void initTimer();
  void activateForwardTimer();
  void receiveForwardMessage(DibadawnPacket &packet);
  void detectCycles();
  
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
