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

#include "topology_dibadawn.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "topology_dibadawn_neighbor_container.hh"


CLICK_DECLS;

class BinaryMatrix
{
  bool *matrix;
  size_t dimension;
  bool printDebug;

public:

  BinaryMatrix(size_t n)
  {
    dimension = n;
    printDebug = false;
    matrix = new bool[dimension];
  }

  ~BinaryMatrix()
  {
    delete[](matrix);
  }

  void setTrue(size_t row, size_t col)
  {
    matrix[row * dimension + col] = true;
  }

  void runMarshallAlgo()
  {
    for (size_t col = 0; col < dimension; col++)
    {
      for (size_t row = 0; row < dimension; row++)
      {
        if (matrix[row * dimension + col] == true)
        {
          for (size_t j = 0; j < dimension; j++)
          {
            matrix[row * dimension + j] =
                matrix[row * dimension + j] || matrix[col * dimension + j];
          }
        }
      }
      if (printDebug)
        print();
    }
  }

  void print()
  {
    click_chatter("</Matrix>\n");
    for (size_t i = 0; i < dimension; i++)
    {
      click_chatter("<MatrixRow num='%d'>", i);
      for (size_t k = 0; k < dimension; k++)
      {
        click_chatter("%d ", matrix[i * dimension + k]);
      }
      click_chatter("</MatrixRow>\n");
    }
    click_chatter("</Matrix>\n");
  }

  bool isOneMatrix()
  {
    for (size_t col = 0; col < dimension; col++)
    {
      for (size_t row = 0; row < dimension; row++)
      {
        if (matrix[row * dimension + col] == false)
          return (false);
      }
    }
    return (true);
  }
};

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

DibadawnSearch::DibadawnSearch(BRNElement *click_element, const EtherAddress &addrOfThisNode)
{
  initCommon(click_element, addrOfThisNode);
}

DibadawnSearch::DibadawnSearch(BRNElement *click_element, const EtherAddress &addrOfThisNode, DibadawnSearchId &id)
{
  initCommon(click_element, addrOfThisNode);

  searchId = id;
  visited = false;
}

void DibadawnSearch::initCommon(BRNElement *click_element, const EtherAddress &thisNode)
{
  brn_click_element = click_element;
  addrOfThisNode = thisNode;
  maxTraversalTimeMs = 40;
  maxTtl = 255; // TODO: Does this makes sense?
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
  detectAccessPoints();
  voteForAccessPointsAndBridges();
}

void DibadawnSearch::detectCycles()
{
  for (int i = 0; i < crossEdges.size(); i++)
  {
    DibadawnPacket &relevantPacket = *crossEdges.at(i);

    DibadawnEdgeMarking marking;
    marking.time = Timestamp::now();
    marking.id = searchId;
    marking.isBridge = false;
    marking.nodeA = addrOfThisNode;
    marking.nodeB = relevantPacket.forwardedBy;
    edgeMarkings.push_back(marking);

    DibadawnCycle c(searchId, addrOfThisNode, relevantPacket.forwardedBy);
    click_chatter("<Cycle id='%s' />", c.AsString().c_str());
    
    DibadawnPayloadElement payload(c);
    relevantPacket.payload.push_back(payload);
    bufferBackwardMessage(c);
  }
}

void DibadawnSearch::forwardMessages()
{
  if(isParentNull())
  {
    click_chatter("<Finished />");
    return;
  }
  
  if (messageBuffer.size() == 0)
  {
    addBridgeEdgeMarking(addrOfThisNode, parent);

    outgoingPacket.searchId = searchId;
    outgoingPacket.forwardedBy = addrOfThisNode;
    outgoingPacket.treeParent = parent;
    outgoingPacket.isForward = false;
    outgoingPacket.ttl = maxTtl;
    outgoingPacket.addBridgeAsPayload();
    sendTo(outgoingPacket, parent);
  }
  else
  {
    DibadawnPacket packet(searchId, addrOfThisNode, false);
    packet.ttl = maxTtl;
    packet.treeParent = parent;
    packet.forwardedBy = addrOfThisNode;
    
    for (int i = 0; i < messageBuffer.size(); i++)
    {
      packet.copyPayloadIfNecessary(messageBuffer.at(i));
      
      // TODO votes

      DibadawnEdgeMarking marking;
      marking.time = Timestamp::now();
      marking.id = searchId;
      marking.isBridge = false;
      marking.nodeA = addrOfThisNode;
      marking.nodeB = parent;
      edgeMarkings.push_back(marking);
    }
      
    messageBuffer.clear();
    sendTo(packet, parent);
  }
}

bool DibadawnSearch::isParentNull()
{
  return(parent == EtherAddress());
}

void DibadawnSearch::detectAccessPoints()
{
  click_chatter("<NotImplemented node='%s' method='detectAccessPoints' />",
      addrOfThisNode.unparse_dash().c_str());
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
  outgoingPacket.searchId = searchId;
  outgoingPacket.forwardedBy = addrOfThisNode;
  outgoingPacket.treeParent = parent;
  outgoingPacket.isForward = true;
  outgoingPacket.ttl = maxTtl;
  visited = true;

  sendBroadcastWithTimeout(outgoingPacket);
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
  
  forwardSendTimer->assign(forwardSendTimerCallback, (void*)param);
  forwardSendTimer->initialize(this->brn_click_element, false);
  forwardSendTimer->schedule_after_msec((click_random() % numOfConcurrentSenders) * maxTraversalTimeMs);
}

bool DibadawnSearch::isResponsibleFor(DibadawnPacket &packet)
{
  return (packet.searchId.isEqualTo(packet.searchId));
}

void DibadawnSearch::receive(DibadawnPacket &rxPacket)
{
  DibadawnNeighbor &neighbor = adjacents.getNeighbor(rxPacket.forwardedBy);
  neighbor.messages.push_back(rxPacket);
  DibadawnPacket &storedRxPacket = neighbor.messages.back();
  storedRxPacket.logRx(addrOfThisNode);

  if (storedRxPacket.isForward)
    receiveForwardMessage(storedRxPacket);
  else
    receiveBackMessage(storedRxPacket);
}

void DibadawnSearch::receiveForwardMessage(DibadawnPacket &rxPacket)
{
  if (!visited)
  {
    if (rxPacket.ttl > 0)
    {
      parent = rxPacket.forwardedBy;
      outgoingPacket = rxPacket;
      outgoingPacket.ttl--;
      outgoingPacket.forwardedBy = addrOfThisNode;
      outgoingPacket.treeParent = rxPacket.forwardedBy;
      visited = true;

      sendDelayedBroadcastWithTimeout(outgoingPacket);
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
  if(packet.payload.size() < 1)
  {
    click_chatter("<Error node='%s' methode='receiveBackMessage' msg='no payload data at back-msg' />",
        addrOfThisNode.unparse_dash().c_str());
    return;
  }
  
  if(packet.hasBridgePayload())
  {
    addBridgeEdgeMarking(addrOfThisNode, packet.forwardedBy);
  }
  else
  {
    pairCyclesIfPossible(packet);
    addPayloadElementsToMessagePuffer(packet);
  }
}

void DibadawnSearch::addBridgeEdgeMarking(EtherAddress &nodeA, EtherAddress &nodeB)
{
  DibadawnEdgeMarking marking;
  marking.time = Timestamp::now();
  marking.id = searchId;
  marking.isBridge = true;
  marking.nodeA = nodeA;
  marking.nodeB = nodeB;
  edgeMarkings.push_back(marking);

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
        edgeMarkings.push_back(marking);

        marking.time = Timestamp::now();
        marking.id = packet.searchId;
        marking.isBridge = false;
        marking.nodeA = addrOfThisNode;
        marking.nodeB = packet.forwardedBy;
        edgeMarkings.push_back(marking);

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
  for(int i = 0; i < packet.payload.size(); i++)
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

void DibadawnSearch::AccessPointDetection()
{
  size_t n = adjacents.numOfNeighbors();
  if (n < 1)
    return;

  BinaryMatrix m(n);
  for (size_t i=0; i<n; i++)
  {
    for(size_t j=i; j<n; j++)
    {
      DibadawnNeighbor& n1 = adjacents.getNeighbor(i);
      DibadawnNeighbor& n2 = adjacents.getNeighbor(j);
      
      if(n1.hasNonEmptyIntersection(n2))
      {
        m.setTrue(j,i);
        m.setTrue(i,j);
      }
    }
  }
  m.runMarshallAlgo();
  isArticulationPoint = !m.isOneMatrix();
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnSearch)
