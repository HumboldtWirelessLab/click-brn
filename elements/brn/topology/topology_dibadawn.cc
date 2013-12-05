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
#include "topology_dibadawn.hh"
#include "topology_dibadawn_packet.hh"
#include "topology_dibadawn_cycle.hh"


CLICK_DECLS;


DibadawnSearch::DibadawnSearch(BRNElement *click_element, const EtherAddress &addrOfThisNode)
{
  brn_click_element = click_element;
  thisNode = addrOfThisNode;
  maxTraversalTimeMs = 40;
  maxTtl = 255; // TODO: Does this makes sense?
  inactive = true;

  initTimer();
}

DibadawnSearch::DibadawnSearch(BRNElement *click_element, const EtherAddress &addrOfThisNode, DibadawnPacket &packet)
{
  brn_click_element = click_element;
  thisNode = addrOfThisNode;
  maxTraversalTimeMs = 40;
  maxTtl = 255; // TODO: Does this makes sense?
  inactive = false;

  outgoingPacket = packet;
  outgoingPacket.forwardedBy = thisNode;
  visited = false;

  initTimer();
}

void forwardTimerCallback(Timer*, void *search)
{
  DibadawnSearch* s = (DibadawnSearch*) search;
  assert(s != NULL);
  s->forwardTimeout();
}

void DibadawnSearch::initTimer()
{
  forwardTimer = new Timer(forwardTimerCallback, this);
}

void DibadawnSearch::activateForwardTimer()
{
  forwardTimer->initialize(this->brn_click_element, false);
  forwardTimer->schedule_after_msec(maxTraversalTimeMs * outgoingPacket.ttl);
}

void DibadawnSearch::forwardTimeout()
{
  const char *searchText = outgoingPacket.searchId.AsString().c_str();
  const char *thisNodeText = thisNode.unparse_dash().c_str();
  click_chatter("<ForwardTimeout searchId='%s' node='%s' />",
      searchText,
      thisNodeText);

  detectCycles();
  forwardMessages();
  detectAccessPoints();
  voteForAccessPointsAndBridges();
  
  inactive = true;
}

void DibadawnSearch::detectCycles()
{
  for(int i = 0; i < crossEdges.size(); i++)
  {
    EtherAddress addr = crossEdges.at(i);
    
    DibadawnEdgeMarking marking = DibadawnEdgeMarking(
        Timestamp::now(), 
        outgoingPacket.searchId,
        true,
        thisNode,
        addr);
    edgeMarkings.push_back(marking);

    DibadawnCycle c(outgoingPacket.searchId, thisNode, addr);
    click_chatter("<Cycle id='%s' />",
        c.AsString().c_str());
    bufferBackwardMessage(addr, c);
  }
}

void DibadawnSearch::forwardMessages()
{
  click_chatter("<NotImplemented method='forwardMessages' />");
}

void DibadawnSearch::detectAccessPoints()
{
  click_chatter("<NotImplemented method='detectAccessPoints' />");
}

void DibadawnSearch::voteForAccessPointsAndBridges()
{
  click_chatter("<NotImplemented method='voteForAccessPointsAndBridges' />");
}

String DibadawnSearch::asString()
{
  return (outgoingPacket.searchId.AsString());
}

void DibadawnSearch::start_search()
{
  outgoingPacket.searchId = DibadawnSearchId(Timestamp::now(), thisNode);
  outgoingPacket.forwardedBy = thisNode;
  outgoingPacket.treeParent = parent;
  outgoingPacket.isForward = true;
  outgoingPacket.ttl = maxTtl; 
  visited = true;
  
  sendPerBroadcastWithTimeout();
}

void DibadawnSearch::sendPerBroadcastWithTimeout()
{
  outgoingPacket.log("DibadawnPacketTx", thisNode);
  WritablePacket *brn_packet = outgoingPacket.getBrnPacket();
  brn_click_element->output(0).push(brn_packet);

  activateForwardTimer();
}

bool DibadawnSearch::isResponsableFor(DibadawnPacket &packet)
{
  return (packet.searchId.isEqualTo(packet.searchId));
}

void DibadawnSearch::receive(DibadawnPacket &receivedPacket)
{
  receivedPacket.log("DibadawnPacketRx", thisNode);
  if (receivedPacket.isForward)
  {
    receiveForwardMessage(receivedPacket);
  }
  else
  {
    click_chatter("<NotImplemented  node='%s' />",
        thisNode.unparse_dash().c_str());
  }
}

void DibadawnSearch::receiveForwardMessage(DibadawnPacket &receivedPacket)
{
  if (!visited)
  {
    if (receivedPacket.ttl > 0)
    {
      outgoingPacket = receivedPacket;
      outgoingPacket.ttl--;
      outgoingPacket.forwardedBy = thisNode;
      outgoingPacket.treeParent = receivedPacket.forwardedBy;
      visited = true;
      
      sendPerBroadcastWithTimeout();
      
      inactive = false;
    }
    else
    {
      click_chatter("<IgnorePacket node='%s' reason='lowTtl' />",
          thisNode.unparse_dash().c_str());
    }
  }
  else if (receivedPacket.treeParent == thisNode)
  {
    click_chatter("<IgnorePacket node='%s' reason='reForward' />",
        thisNode.unparse_dash().c_str());
  }
  else
  {
    EtherAddress neighbor = receivedPacket.forwardedBy;

    click_chatter("<CrossEdgeDetected  node='%s' neigbor='%s'/>",
        thisNode.unparse_dash().c_str(),
        neighbor.unparse_dash().c_str());

    crossEdges.push_back(neighbor);
  }
}

void DibadawnSearch::bufferBackwardMessage(EtherAddress& parent  /*wozu?*/, DibadawnCycle &cycleId)
{
  DibadawnPacket packet;
  packet.isForward = false;
  packet.forwardedBy = thisNode;
  packet.treeParent = parent;
  packet.ttl = maxTtl;
  packet.containsPayload = true;
  packet.isPayloadBridge = false;
  packet.payload = cycleId;
  
  messageBuffer.push_back(packet);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnSearch)
