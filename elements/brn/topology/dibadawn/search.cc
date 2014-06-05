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
    DibadawnNodeStatistic &stat, 
    const EtherAddress &addrOfThisNode)
:   commonStatistic(stat), adjacents(searchId)
{
  initCommon(click_element, addrOfThisNode);
}

DibadawnSearch::DibadawnSearch(
    BRNElement *click_element,
    DibadawnNodeStatistic &stat, 
    const EtherAddress &addrOfThisNode,
    DibadawnSearchId &id)
:   commonStatistic(stat), adjacents(searchId)
{
  initCommon(click_element, addrOfThisNode);

  searchId.set(id);
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
  
  click_chatter("<ForwardTimeout node='%s' searchId='%s' />", 
      addrOfThisNode.unparse_dash().c_str(), 
      searchId.asString().c_str());

  detectCycles();
  forwardMessages();
  detectArticulationPoints();
  voteForArticulaionPointsAndBridges();
}

void DibadawnSearch::detectCycles()
{
  for (int i = 0; i < crossEdges.size(); i++)
  {
    DibadawnPacket &crossEdgePacket = crossEdges.at(i);

    DibadawnEdgeMarking marking(searchId, false, addrOfThisNode, crossEdgePacket.forwardedBy, true);
    commonStatistic.updateEdgeMarking(marking);

    DibadawnCycle cycle(searchId, addrOfThisNode, crossEdgePacket.forwardedBy);
    click_chatter("<Cycle id='%s' />", cycle.asString().c_str());

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
    DibadawnPayloadElement bridge(searchId, addrOfThisNode, parentNode, true);
    
    DibadawnPacket packet(searchId, addrOfThisNode, false);
    packet.treeParent = parentNode;
    packet.ttl = maxTtl;
    packet.payload.push_back(bridge);
    sendTo(packet, parentNode);
    
    uint8_t hops = getUsedHops(packet.ttl);
    double competence = commonStatistic.competenceByUsedHops(hops);
    addBridgeEdgeMarking(addrOfThisNode, parentNode, competence);
    
    DibadawnNeighbor &neighbor = adjacents.getNeighbor(parentNode);
    neighbor.messages.push_back(bridge);
  }
  else
  {
    DibadawnPacket packet(searchId, addrOfThisNode, false);
    packet.ttl = maxTtl;
    packet.treeParent = parentNode;
    packet.forwardedBy = addrOfThisNode;

    DibadawnNeighbor &neighbor = adjacents.getNeighbor(parentNode);
    for (int i = 0; i < messageBuffer.size(); i++)
    {
      DibadawnPayloadElement &payload = messageBuffer.at(i);
      packet.copyPayloadIfNecessary(payload);
      neighbor.copyPayloadIfNecessary(payload);

      // TODO votes

      DibadawnEdgeMarking marking(searchId, false, addrOfThisNode, parentNode, false);
      commonStatistic.updateEdgeMarking(marking);
    }

    messageBuffer.clear();
    sendTo(packet, parentNode);
  }
}

bool DibadawnSearch::isParentNull()
{
  return (parentNode == EtherAddress());
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
      if (nodeA.hasNonEmptyPairIntersection(nodeB))
      {
        nodeA.printIntersection(addrOfThisNode, nodeB);
        closure.setTrue(row, col);
      }
    }
  }

  closure.print(addrOfThisNode.unparse().c_str());
  closure.runMarshallAlgo();
  closure.print(addrOfThisNode.unparse().c_str());

  bool isArticulationPoint = !closure.isOneMatrix();
  if (isArticulationPoint)
  {
    click_chatter("<ArticulationPoint node='%s'/>",
        addrOfThisNode.unparse_dash().c_str());
    
  }
  commonStatistic.upateArticulationPoint(addrOfThisNode, isArticulationPoint);
}

void DibadawnSearch::voteForArticulaionPointsAndBridges()
{
  click_chatter("<NotImplemented node='%s' method='voteForArticulaionPointsAndBridges' />",
      addrOfThisNode.unparse_dash().c_str());
}

void DibadawnSearch::start_search()
{
  setParentNull();

  searchId.set(Timestamp::now(), addrOfThisNode);
  DibadawnPacket packet(searchId, addrOfThisNode, true);
  packet.treeParent = parentNode;
  packet.ttl = maxTtl;
  visited = true;

  sendBroadcastWithTimeout(packet);
}

void DibadawnSearch::setParentNull()
{
  parentNode = EtherAddress();
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
  uint32_t random_number = click_random();
  uint32_t delay = (random_number % numOfConcurrentSenders) * maxTraversalTimeMs;
  click_chatter("<DEBUG node='%s' random='%d' txDelay='%d' />", addrOfThisNode.unparse().c_str(), random_number, delay);
  forwardSendTimer->schedule_after_msec(delay);
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
      parentNode = rxPacket.forwardedBy;
      visited = true;
      adjacents.getNeighbor(parentNode);
      
      click_chatter("<SearchTree node='%s' parent='%s' time='%s' searchId='%s' />",
          addrOfThisNode.unparse().c_str(),
          parentNode.unparse().c_str(),
          Timestamp::now().unparse().c_str(),
          searchId.asString().c_str());

      DibadawnPacket txPacket = rxPacket;
      txPacket.ttl--;
      txPacket.forwardedBy = addrOfThisNode;
      txPacket.treeParent = rxPacket.forwardedBy;

      sendDelayedBroadcastWithTimeout(txPacket);
    }
    else
    {
      click_chatter("<IgnorePacket node='%s' reason='lowTtl' time='%s' searchId='%s' />",
          addrOfThisNode.unparse_dash().c_str(),
          Timestamp::now().unparse().c_str(),
          searchId.asString().c_str());
    }
  }
  else if (rxPacket.treeParent == addrOfThisNode)
  {
    click_chatter("<IgnorePacket node='%s' reason='reForward' time='%s' searchId='%s' />",
        addrOfThisNode.unparse_dash().c_str(),
        Timestamp::now().unparse().c_str(),
        searchId.asString().c_str());
  }
  else
  {
    EtherAddress neighbor = rxPacket.forwardedBy;

    click_chatter("<CrossEdgeDetected  node='%s' neighbor='%s' time='%s' searchId='%s' />",
        addrOfThisNode.unparse_dash().c_str(),
        neighbor.unparse_dash().c_str(),
        Timestamp::now().unparse().c_str(),
        searchId.asString().c_str());

    crossEdges.push_back(rxPacket);
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
    for (int i = 0; i < packet.payload.size(); i++)
    {
      DibadawnPayloadElement &payloadA = packet.payload.at(i);
      DibadawnPayloadElement *payloadB = tryToFindPair(payloadA);
      if(payloadB != NULL)
      {
        click_chatter("<PairedCycle node='%s' time='%s' searchId='%s' cycle='%s'/>",
            addrOfThisNode.unparse_dash().c_str(),
            Timestamp::now().unparse().c_str(),
            searchId.asString().c_str(),
            payloadA.cycle.asString().c_str());

        setNonBrigdeByPayload(payloadA);

        removePayloadFromMessageBuffer(payloadA);
      }
      else
      {
        messageBuffer.push_back(payloadA);
      } 
    }
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

DibadawnPayloadElement* DibadawnSearch::tryToFindPair(DibadawnPayloadElement& payloadA)
{
  for (Vector<DibadawnPayloadElement>::iterator it = messageBuffer.begin(); it != messageBuffer.end(); it++)
  {
    DibadawnPayloadElement& paylaodB = *it;
    if (payloadA.cycle == paylaodB.cycle)
    {
      return(&paylaodB);
    }
  }
  return(NULL);
}

void DibadawnSearch::setNonBrigdeByPayload(DibadawnPayloadElement& payload)
{
  for(size_t i = 0; i < adjacents.numOfNeighbors(); i++)
  {
    DibadawnNeighbor & neighbor = adjacents.getNeighbor(i);
    if(neighbor.hasSameCycle(payload))
    {
      DibadawnEdgeMarking marking;
      marking.time = Timestamp::now();
      marking.id = searchId;
      marking.isBridge = false;
      marking.nodeA = addrOfThisNode;
      marking.nodeB = neighbor.address;
      
      commonStatistic.updateEdgeMarking(marking);
    }
  }
}

void DibadawnSearch::removePayloadFromMessageBuffer(DibadawnPayloadElement& payloadA)
{
  for (Vector<DibadawnPayloadElement>::iterator it = messageBuffer.begin(); it != messageBuffer.end(); it++)
  {
    DibadawnPayloadElement& paylaodB = *it;
    if (payloadA.cycle == paylaodB.cycle)
    {
      messageBuffer.erase(it);
      break;
    }
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
