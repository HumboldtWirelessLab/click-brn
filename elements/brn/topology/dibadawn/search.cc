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
#include "elements/standard/randomerror.hh"


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
    DibadawnConfig &cfg)
:   commonStatistic(stat), adjacents(searchId), config(cfg)
{
  initCommon(click_element);
}

DibadawnSearch::DibadawnSearch(
    BRNElement *click_element,
    DibadawnNodeStatistic &stat, 
    DibadawnConfig &cfg,
    DibadawnSearchId &id)
:   commonStatistic(stat), adjacents(searchId), config(cfg)
{
  initCommon(click_element);

  searchId.set(id);
  visited = false;
}

void DibadawnSearch::initCommon(
    BRNElement *click_element)
{
  brn_click_element = click_element;
  isArticulationPoint = false;
  numOfConcurrentSenders = 10;

  forwardTimeoutTimer = new Timer(forwardTimeoutCallback, this);
  forwardSendTimer = new Timer(forwardSendTimerCallback, NULL);
}

void DibadawnSearch::forwardTimeout()
{
  if(config.debugLevel > 0)
    click_chatter("<ForwardTimeout node='%s' searchId='%s' />",
      config.thisNodeAsCstr(),
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

    DibadawnEdgeMarking marking(searchId, false, config.thisNode, crossEdgePacket.forwardedBy, true);
    commonStatistic.updateEdgeMarking(marking);

    DibadawnCycle cycle(searchId, config.thisNode, crossEdgePacket.forwardedBy);
    if(config.debugLevel > 0)
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
    if(config.debugLevel > 0)
      click_chatter("<Finished node='%s' time='%s' searchId='%s' />",
            config.thisNodeAsCstr(),
            Timestamp::now().unparse().c_str(),
            searchId.asString().c_str());
    return;
  }

  if (messageBuffer.size() == 0)
  {
    DibadawnPayloadElement bridge(searchId, config.thisNode, parentNode, true);
    
    DibadawnPacket packet(searchId, config.thisNode, false);
    packet.treeParent = parentNode;
    packet.hops = 1;
    packet.payload.push_back(bridge);
    sendTo(packet, parentNode);
    
    double competence = commonStatistic.competenceByUsedHops(packet.hops);
    addBridgeEdgeMarking(config.thisNode, parentNode, competence, "self");
    
    DibadawnNeighbor &neighbor = adjacents.getNeighbor(parentNode);
    neighbor.messages.push_back(bridge);
  }
  else
  {
    DibadawnPacket packet(searchId, config.thisNode, false);
    packet.hops = 1;
    packet.treeParent = parentNode;
    packet.forwardedBy = config.thisNode;

    DibadawnNeighbor &neighbor = adjacents.getNeighbor(parentNode);
    for (int i = 0; i < messageBuffer.size(); i++)
    {
      DibadawnPayloadElement &payload = messageBuffer.at(i);
      packet.copyPayloadIfNecessary(payload);
      neighbor.copyPayloadIfNecessary(payload);

      // TODO votes

      DibadawnEdgeMarking marking(searchId, false, config.thisNode, parentNode, false);
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
        nodeA.printIntersection(config.thisNode, nodeB);
        closure.setTrue(row, col);
      }
    }
  }

  if(config.debugLevel > 1)
    closure.print(config.thisNodeAsCstr());
  closure.runMarshallAlgo();
  if(config.debugLevel > 1)
    closure.print(config.thisNodeAsCstr());

  bool isArticulationPoint = !closure.isOneMatrix();
  if (isArticulationPoint)
  {
    if(config.debugLevel > 0)
    click_chatter("<ArticulationPoint node='%s'/>",
        config.thisNodeAsCstr());
  }
  commonStatistic.upateArticulationPoint(config.thisNode, isArticulationPoint);
}

void DibadawnSearch::voteForArticulaionPointsAndBridges()
{
  click_chatter("<NotImplemented node='%s' method='voteForArticulaionPointsAndBridges' />",
      config.thisNodeAsCstr());
}

void DibadawnSearch::start_search()
{
  setParentNull();

  searchId.set(Timestamp::now(), config.thisNode);
  DibadawnPacket packet(searchId, config.thisNode, true);
  packet.treeParent = parentNode;
  packet.hops = 1;
  visited = true;

  if(config.debugLevel > 0)
    click_chatter("<DibadawnStartSearch node='%s'/>",
        config.thisNodeAsCstr());
  
  sendBroadcastWithTimeout(packet);
}

void DibadawnSearch::setParentNull()
{
  parentNode = EtherAddress();
}

void DibadawnSearch::sendBroadcastWithTimeout(DibadawnPacket &packet)
{
  packet.logTx(config.thisNode, packet.getBroadcastAddress());
  WritablePacket *brn_packet = packet.getBrnBroadcastPacket();
  brn_click_element->output(0).push(brn_packet);

  activateForwardTimer(packet);
}

void DibadawnSearch::activateForwardTimer(DibadawnPacket &packet)
{
  forwardTimeoutTimer->initialize(this->brn_click_element, false);
  forwardTimeoutTimer->schedule_after_msec(numOfConcurrentSenders * config.maxTraversalTimeMs * (config.maxHops - packet.hops));
}

void DibadawnSearch::sendTo(DibadawnPacket &packet, EtherAddress &dest)
{
  packet.logTx(config.thisNode, dest);
  WritablePacket *brn_packet = packet.getBrnPacket(dest);
  brn_click_element->output(0).push(brn_packet);
}

void DibadawnSearch::sendDelayedBroadcastWithTimeout(DibadawnPacket &packet)
{
  activateForwardSendTimer(packet);
}

void DibadawnSearch::activateForwardSendTimer(DibadawnPacket &packet)
{
  uint16_t delay = config.useOriginForwardDelay? calcForwardDelay(): calcForwardDelayImproved(packet);
  packet.sumForwardDelay = (packet.sumForwardDelay + delay) % 65535;
  
  
  ForwardSendTimerParam *param = new ForwardSendTimerParam;
  param->packet = packet;
  param->search = this;

  forwardSendTimer->assign(forwardSendTimerCallback, (void*) param);
  forwardSendTimer->initialize(this->brn_click_element, false);
  
  if(config.debugLevel > 1)
    click_chatter("<DEBUG node='%s' txDelay='%d' />", config.thisNodeAsCstr(), delay);
  forwardSendTimer->schedule_after_msec(delay);
}

uint16_t DibadawnSearch::calcForwardDelay()
{
  uint16_t randomNumber = click_random();
  uint16_t delay = (randomNumber % numOfConcurrentSenders) * config.maxTraversalTimeMs;
  return(delay);
}

uint16_t DibadawnSearch::calcForwardDelayImproved(DibadawnPacket &packet)
{
  int hops = packet.hops - 1 /* ignore first hop (delayed with 0ms) */;
  uint16_t randomNumber = click_random();
  uint16_t currentMax = 2 * ((hops * config.maxTraversalTimeMs/2) - packet.sumForwardDelay);
  
  if(config.debugLevel > 2)
    click_chatter("<DEBUG node='%s' rand_min='0' rand_max='%d' />", config.thisNodeAsCstr(), currentMax);
  
  return(randomNumber % currentMax);
}

bool DibadawnSearch::isResponsibleFor(DibadawnPacket &packet)
{
  return (packet.searchId.isEqualTo(packet.searchId));
}

void DibadawnSearch::receive(DibadawnPacket &rxPacket)
{
  if(config.debugLevel > 0)
    rxPacket.logRx(config.thisNode);

  if (rxPacket.isForward)
    receiveForwardMessage(rxPacket);
  else
    receiveBackMessage(rxPacket);
}

void DibadawnSearch::receiveForwardMessage(DibadawnPacket &rxPacket)
{
  if (!visited)
  {
    if (rxPacket.hops < config.maxHops)
    {
      parentNode = rxPacket.forwardedBy;
      visited = true;
      adjacents.getNeighbor(parentNode);
      
      if(config.debugLevel > 0)
        click_chatter("<SearchTree node='%s' parent='%s' time='%s' searchId='%s' />",
          config.thisNodeAsCstr(),
          parentNode.unparse().c_str(),
          Timestamp::now().unparse().c_str(),
          searchId.asString().c_str());

      DibadawnPacket txPacket = rxPacket;
      txPacket.hops++;
      txPacket.forwardedBy = config.thisNode;
      txPacket.treeParent = rxPacket.forwardedBy;
      sentForwardPacket = txPacket;

      sendDelayedBroadcastWithTimeout(txPacket);
    }
    else
    {
      if(config.debugLevel > 0)
        click_chatter("<IgnorePacket node='%s' reason='lowTtl' time='%s' searchId='%s' />",
          config.thisNodeAsCstr(),
          Timestamp::now().unparse().c_str(),
          searchId.asString().c_str());
    }
  }
  else if (rxPacket.treeParent == config.thisNode)
  {
    if(config.debugLevel > 0)
      click_chatter("<IgnorePacket node='%s' reason='reForward' time='%s' searchId='%s' />",
        config.thisNodeAsCstr(),
        Timestamp::now().unparse().c_str(),
        searchId.asString().c_str());
  }
  else
  {
    bool isValid = isValidCrossEdge(rxPacket);
    EtherAddress neighbor = rxPacket.forwardedBy;
    
    if(config.debugLevel > 0)
      click_chatter("<CrossEdgeDetected  node='%s' neighbor='%s' time='%s' searchId='%s' valid='%d' />",
        config.thisNodeAsCstr(),
        neighbor.unparse_dash().c_str(),
        Timestamp::now().unparse().c_str(),
        searchId.asString().c_str(),
        isValid);
    
    if(isValid)
      crossEdges.push_back(rxPacket);
  }
}

bool DibadawnSearch::isValidCrossEdge(DibadawnPacket& rxPacket)
{
  return(abs(sentForwardPacket.hops - rxPacket.hops) <= 1);
}


void DibadawnSearch::receiveBackMessage(DibadawnPacket& packet)
{
  if (packet.payload.size() < 1)
  {
    click_chatter("<Error node='%s' methode='receiveBackMessage' msg='no payload data at back-msg' />",
        config.thisNodeAsCstr());
    return;
  }

  if (packet.hasBridgePayload())
  {
    double competence = commonStatistic.competenceByUsedHops(packet.hops);
    addBridgeEdgeMarking(packet.forwardedBy, config.thisNode, competence, "adjacent");

    DibadawnPayloadElement bridge(searchId, config.thisNode, packet.forwardedBy, true);
    DibadawnNeighbor &neighbor = adjacents.getNeighbor(packet.forwardedBy);
    neighbor.messages.push_back(bridge);
  }
  else
  {
    updateAdjacent(packet);
    
    for (int i = 0; i < packet.payload.size(); i++)
    {
      DibadawnPayloadElement &payloadA = packet.payload.at(i);
      DibadawnPayloadElement *payloadB = tryToFindPair(payloadA);
      if(payloadB != NULL)
      {
        click_chatter("<PairedCycle node='%s' time='%s' searchId='%s' cycle='%s'/>",
            config.thisNodeAsCstr(),
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

void DibadawnSearch::addBridgeEdgeMarking(EtherAddress &nodeA, EtherAddress &nodeB, double competence, const char *src)
{
  DibadawnEdgeMarking marking(searchId, true, nodeA, nodeB, competence);
  commonStatistic.updateEdgeMarking(marking);

  click_chatter("<Bridge node='%s' nodeA='%s' nodeB='%s' src='%s' />",
      config.thisNodeAsCstr(),
      nodeA.unparse_dash().c_str(),
      nodeB.unparse_dash().c_str(),
      src);
}

void DibadawnSearch::updateAdjacent(DibadawnPacket& packet)
{
  for (int i = 0; i < packet.payload.size(); i++)
  {
    DibadawnNeighbor &neighbor = adjacents.getNeighbor(packet.forwardedBy);
    neighbor.messages.push_back(packet.payload.at(i));
  }
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
      marking.nodeA = config.thisNode;
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


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnSearch)
