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

#include "search.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "neighbor_container.hh"
#include "binarymatrix.hh"


CLICK_DECLS;

void forwardSendTimerCallback(Timer*, void* p)
{
  DibadawnSearch::ForwardSendTimerParam *param = (DibadawnSearch::ForwardSendTimerParam *)p;
  assert(param != NULL);

  param->search->sendBroadcastWithTimeout(param->packet);
  delete(param);
}

void forwardTimeoutCallback(Timer*, void *search)
{
  DibadawnSearch* s = (DibadawnSearch*) search;
  assert(s != NULL);
  s->forwardTimeout();
}

DibadawnSearch::DibadawnSearch(
    BRNElement *click_element,
    const EtherAddress &addrOfThisNode)
:   commonStatistic(DibadawnStatistic::getInstance())
{
  initCommon(click_element, addrOfThisNode);
}

DibadawnSearch::DibadawnSearch(
    BRNElement *click_element,
    const EtherAddress &addrOfThisNode,
    DibadawnSearchId &id)
:   commonStatistic(DibadawnStatistic::getInstance())
{
  initCommon(click_element, addrOfThisNode);

  searchId = id;
  visited = false;
}

void DibadawnSearch::initCommon(
    BRNElement *click_element,
    const EtherAddress &thisNode)
{
  brn_click_element = click_element;
  addrOfThisNode = thisNode;
  maxTraversalTimeMs = 40;
  maxTtl = 255; // TODO: Use config instead
  isArticulationPoint = false;
  numOfConcurrentSenders = 10;

  forwardTimeoutTimer = new Timer(forwardTimeoutCallback, this);
  forwardSendTimer = new Timer(forwardSendTimerCallback, NULL);
}

void DibadawnSearch::forwardTimeout()
{
  const char *searchText = searchId.AsString().c_str();
  const char *thisNodeText = addrOfThisNode.unparse_dash().c_str();
  click_chatter("<ForwardTimeout node='%s' searchId='%s' />", thisNodeText, searchText);

  detectCycles();
  forwardMessages();
  detectArticulationPoints();
  voteForAccessPointsAndBridges();
}

void DibadawnSearch::detectCycles()
{
  for (int i = 0; i < crossEdges.size(); i++)
  {
    DibadawnPacket &crossEdgePacket = *crossEdges.at(i);

    DibadawnEdgeMarking marking(searchId, false, addrOfThisNode, crossEdgePacket.forwardedBy, true);
    commonStatistic.updateEdgeMarking(marking);

    DibadawnCycle cycle(searchId, addrOfThisNode, crossEdgePacket.forwardedBy);
    click_chatter("<Cycle id='%s' />", cycle.AsString().c_str());

    DibadawnNeighbor &neighbor = adjacents.getNeighbor(crossEdgePacket.forwardedBy);
    DibadawnPayloadElement payload(cycle);
    neighbor.messages.push_back(cycle);

    bufferBackwardMessage(cycle);
  }
}

void DibadawnSearch::forwardMessages()
{
  if (isParentNull())
  {
    click_chatter("<Finished />");
    return;
  }

  if (messageBuffer.size() == 0)
  {
    DibadawnPayloadElement bridge(searchId, addrOfThisNode, parent, true);
    
    DibadawnPacket packet(searchId, addrOfThisNode, false);
    packet.treeParent = parent;
    packet.ttl = maxTtl;
    packet.payload.push_back(bridge);
    sendTo(packet, parent);
    
    uint8_t hops = getUsedHops(packet.ttl);
    double competence = commonStatistic.competenceByUsedHops(hops);
    addBridgeEdgeMarking(addrOfThisNode, parent, competence);
    
    DibadawnNeighbor &neighbor = adjacents.getNeighbor(parent);
    neighbor.messages.push_back(bridge);
  }
  else
  {
    DibadawnPacket packet(searchId, addrOfThisNode, false);
    packet.ttl = maxTtl;
    packet.treeParent = parent;
    packet.forwardedBy = addrOfThisNode;

    DibadawnNeighbor &neighbor = adjacents.getNeighbor(parent);
    for (int i = 0; i < messageBuffer.size(); i++)
    {
      DibadawnPayloadElement &payload = messageBuffer.at(i);
      packet.copyPayloadIfNecessary(payload);
      neighbor.copyPayloadIfNecessary(payload);

      // TODO votes

      DibadawnEdgeMarking marking(searchId, false, addrOfThisNode, parent, false);
       commonStatistic.updateEdgeMarking(marking);
    }

    messageBuffer.clear();
    sendTo(packet, parent);
  }
}

bool DibadawnSearch::isParentNull()
{
  return (parent == EtherAddress());
}

void DibadawnSearch::detectArticulationPoints()
{
  size_t num = adjacents.numOfNeighbors();
  BinaryMatrix closure(num);

  for (size_t row = 0; row < num; row++)
  {
    DibadawnNeighbor &nodeA = adjacents.getNeighbor(row);
    for (size_t col = 0; col < num; col++)
    {
      DibadawnNeighbor &nodeB = adjacents.getNeighbor(col);

      if (nodeA.hasNonEmptyIntersection(nodeB))
      {
        nodeA.printIntersection(addrOfThisNode, nodeB);
        closure.setTrue(row, col);
      }
    }
  }

  closure.print(addrOfThisNode.unparse().c_str());
  closure.runMarshallAlgo();
  closure.print(addrOfThisNode.unparse().c_str());

  if (!closure.isOneMatrix())
  {
    click_chatter("<ArticulationPoint node='%s'/>",
        addrOfThisNode.unparse_dash().c_str());
  }
}

void DibadawnSearch::voteForAccessPointsAndBridges()
{
  click_chatter("<NotImplemented node='%s' method='voteForAccessPointsAndBridges' />",
      addrOfThisNode.unparse_dash().c_str());
}

String DibadawnSearch::asString()
{
  return (searchId.AsString());
}

void DibadawnSearch::start_search()
{
  setParentNull();

  searchId = DibadawnSearchId(Timestamp::now(), addrOfThisNode);
  DibadawnPacket packet(searchId, addrOfThisNode, true);
  packet.treeParent = parent;
  packet.ttl = maxTtl;
  visited = true;

  sendBroadcastWithTimeout(packet);
}

void DibadawnSearch::setParentNull()
{
  parent = EtherAddress();
}

void DibadawnSearch::sendBroadcastWithTimeout(DibadawnPacket &packet)
{
  packet.logTx(addrOfThisNode, packet.getBroadcastAddress());
  WritablePacket *brn_packet = packet.getBrnBroadcastPacket();
  brn_click_element->output(0).push(brn_packet);

  activateForwardTimer(packet);
}

void DibadawnSearch::activateForwardTimer(DibadawnPacket &packet)
{
  forwardTimeoutTimer->initialize(this->brn_click_element, false);
  forwardTimeoutTimer->schedule_after_msec(numOfConcurrentSenders * maxTraversalTimeMs * packet.ttl);
}

void DibadawnSearch::sendTo(DibadawnPacket &packet, EtherAddress &dest)
{
  packet.logTx(addrOfThisNode, dest);
  WritablePacket *brn_packet = packet.getBrnPacket(dest);
  brn_click_element->output(0).push(brn_packet);
}

void DibadawnSearch::sendDelayedBroadcastWithTimeout(DibadawnPacket &packet)
{
  activateForwardSendTimer(packet);
}

void DibadawnSearch::activateForwardSendTimer(DibadawnPacket &packet)
{
  ForwardSendTimerParam *param = new ForwardSendTimerParam;
  param->packet = packet;
  param->search = this;

  forwardSendTimer->assign(forwardSendTimerCallback, (void*) param);
  forwardSendTimer->initialize(this->brn_click_element, false);
  forwardSendTimer->schedule_after_msec((click_random() % numOfConcurrentSenders) * maxTraversalTimeMs);
}

bool DibadawnSearch::isResponsibleFor(DibadawnPacket &packet)
{
  return (packet.searchId.isEqualTo(packet.searchId));
}

void DibadawnSearch::receive(DibadawnPacket &rxPacket)
{
  rxPacket.logRx(addrOfThisNode);

  if (rxPacket.isForward)
    receiveForwardMessage(rxPacket);
  else
    receiveBackMessage(rxPacket);
}

void DibadawnSearch::receiveForwardMessage(DibadawnPacket &rxPacket)
{
  if (!visited)
  {
    if (rxPacket.ttl > 0)
    {
      parent = rxPacket.forwardedBy;
      visited = true;
      adjacents.getNeighbor(parent);

      DibadawnPacket txPacket = rxPacket;
      txPacket.ttl--;
      txPacket.forwardedBy = addrOfThisNode;
      txPacket.treeParent = rxPacket.forwardedBy;

      sendDelayedBroadcastWithTimeout(txPacket);
    }
    else
    {
      click_chatter("<IgnorePacket node='%s' reason='lowTtl' />",
          addrOfThisNode.unparse_dash().c_str());
    }
  }
  else if (rxPacket.treeParent == addrOfThisNode)
  {
    click_chatter("<IgnorePacket node='%s' reason='reForward' />",
        addrOfThisNode.unparse_dash().c_str());
  }
  else
  {
    EtherAddress neighbor = rxPacket.forwardedBy;

    click_chatter("<CrossEdgeDetected  node='%s' neighbor='%s'/>",
        addrOfThisNode.unparse_dash().c_str(),
        neighbor.unparse_dash().c_str());

    crossEdges.push_back(&rxPacket);
  }
}

void DibadawnSearch::receiveBackMessage(DibadawnPacket& packet)
{
  if (packet.payload.size() < 1)
  {
    click_chatter("<Error node='%s' methode='receiveBackMessage' msg='no payload data at back-msg' />",
        addrOfThisNode.unparse_dash().c_str());
    return;
  }

  if (packet.hasBridgePayload())
  {
    uint8_t hops = getUsedHops(packet.ttl);
    double competence = commonStatistic.competenceByUsedHops(hops);
    addBridgeEdgeMarking(addrOfThisNode, packet.forwardedBy, competence);

    DibadawnPayloadElement bridge(searchId, addrOfThisNode, packet.forwardedBy, true);
    DibadawnNeighbor &neighbor = adjacents.getNeighbor(packet.forwardedBy);
    neighbor.messages.push_back(bridge);
  }
  else
  {
    pairCyclesIfPossible(packet);
    addPayloadElementsToMessagePuffer(packet);
    
    DibadawnEdgeMarking marking(searchId, false, addrOfThisNode, packet.forwardedBy, true);
    commonStatistic.updateEdgeMarking(marking);
  }
}

void DibadawnSearch::addBridgeEdgeMarking(EtherAddress &nodeA, EtherAddress &nodeB, double competence)
{
  DibadawnEdgeMarking marking(searchId, true, nodeA, nodeB, competence);
  commonStatistic.updateEdgeMarking(marking);

  click_chatter("<Bridge node='%s' nodeA='%s' nodeB='%s' />",
      this->addrOfThisNode.unparse_dash().c_str(),
      nodeA.unparse_dash().c_str(),
      nodeB.unparse_dash().c_str());
}

void DibadawnSearch::pairCyclesIfPossible(DibadawnPacket& packet)
{
  int i = 0;
  while (i < packet.payload.size())
  {
    DibadawnPayloadElement &e = packet.payload.at(i);
    if (e.isBridge)
    {
      click_chatter("<Error node='%s' methode='pairCyclesIfPossible' msg='Payload contains bridge mixed with nobridge-info' />",
          addrOfThisNode.unparse_dash().c_str());
    }
    else
    {
      bool rc = tryToPairPayloadElement(e);
      if (rc)
      {
        DibadawnEdgeMarking marking;
        marking.time = Timestamp::now();
        marking.id = packet.searchId;
        marking.isBridge = false;
        marking.nodeA = addrOfThisNode;
        marking.nodeB = packet.forwardedBy;
        commonStatistic.updateEdgeMarking(marking);

        marking.time = Timestamp::now();
        marking.id = packet.searchId;
        marking.isBridge = false;
        marking.nodeA = addrOfThisNode;
        marking.nodeB = packet.forwardedBy;
        commonStatistic.updateEdgeMarking(marking);

        packet.removeCycle(e);
        continue;
      }
      else
      {
        messageBuffer.push_back(e);
      }
    }

    i++;
  }
}

void DibadawnSearch::addPayloadElementsToMessagePuffer(DibadawnPacket& packet)
{
  for (int i = 0; i < packet.payload.size(); i++)
  {
    DibadawnPayloadElement &elem = packet.payload.at(i);
    messageBuffer.push_back(elem);
  }
}

bool DibadawnSearch::tryToPairPayloadElement(DibadawnPayloadElement& payload1)
{
  for (Vector<DibadawnPayloadElement>::iterator it = messageBuffer.begin(); it != messageBuffer.end(); it++)
  {
    DibadawnPayloadElement& paylaod2 = *it;
    if (payload1.cycle == paylaod2.cycle)
    {
      click_chatter("<PairedCycle node='%s' cycle='%s'/>",
          addrOfThisNode.unparse_dash().c_str(),
          payload1.cycle.AsString().c_str());

      messageBuffer.erase(it);
      return (true);
    }
  }

  return (false);
}

void DibadawnSearch::bufferBackwardMessage(DibadawnCycle &cycleId)
{
  DibadawnPayloadElement elem(cycleId);
  messageBuffer.push_back(elem);
}

uint8_t DibadawnSearch::getUsedHops(uint8_t ttl)
{
  return(maxTtl - ttl);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnSearch)
