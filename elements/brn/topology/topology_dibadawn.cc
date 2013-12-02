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


CLICK_DECLS;


DibadawnSearch::DibadawnSearch(BRNElement *click_element, const EtherAddress &addrOfThisNode)
{
  brn_click_element = click_element;
  thisNode = addrOfThisNode;
  maxTraversalTimeMs = 40;
  inactive = true;
  
  ideaOfPacket.searchId = DibadawnSearchId(Timestamp::now(), thisNode);
  ideaOfPacket.forwardedBy = thisNode;
  ideaOfPacket.isForward = true;
  ideaOfPacket.ttl = 255;  // TODO: Does this makes sense?
  firstProcessedPacket = true;
  
  initTimer();
}

DibadawnSearch::DibadawnSearch(BRNElement *click_element, const EtherAddress &addrOfThisNode, DibadawnPacket &packet)
{
  brn_click_element = click_element;
  thisNode = addrOfThisNode;
  maxTraversalTimeMs = 40;
  inactive = false;
  
  ideaOfPacket = packet;
  ideaOfPacket.forwardedBy = thisNode;
  firstProcessedPacket = true;
  
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
  forwardTimer->schedule_after_msec(maxTraversalTimeMs * ideaOfPacket.ttl);
}

String DibadawnSearch::asString()
{
  return(ideaOfPacket.searchId.AsString());
}

void DibadawnSearch::start_search()
{
  sendPerBroadcastWithTimeout();
  firstProcessedPacket = false;
}

void DibadawnSearch::sendPerBroadcastWithTimeout()
{
  ideaOfPacket.log("DibadawnPacketTx", thisNode);
  WritablePacket *brn_packet = ideaOfPacket.getBrnPacket();
  brn_click_element->output(0).push(brn_packet);
  
  activateForwardTimer();
}

bool DibadawnSearch::isResponsableFor(DibadawnPacket &packet)
{
  return(packet.searchId.isEqualTo(packet.searchId));
}

void DibadawnSearch::receive(DibadawnPacket &receivedPacket)
{
  receivedPacket.log("DibadawnPacketRx", thisNode);
  if(receivedPacket.isForward)
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
  
  if(firstProcessedPacket)
  {
    if(receivedPacket.ttl > 0)
    {
    ideaOfPacket.ttl = receivedPacket.ttl - 1;
    ideaOfPacket.treeParent = receivedPacket.forwardedBy;
    sendPerBroadcastWithTimeout();
    firstProcessedPacket = false;
    inactive = false;
    }
    else
    {
      click_chatter("<IgnorePacket node='%s' reason='lowTtl' />",
        thisNode.unparse_dash().c_str());
    }
  }
  else if(receivedPacket.treeParent == thisNode)
  {
    click_chatter("<IgnorePacket node='%s' reason='reForward' />",
        thisNode.unparse_dash().c_str());
  }
  else
  {
    click_chatter("<CrossEdgeDetected  node='%s' />",
        thisNode.unparse_dash().c_str());
  }
}

void DibadawnSearch::forwardTimeout()
{
  click_chatter("<ForwardTimeout searchId='%s' node='%s' />",
      ideaOfPacket.searchId.AsString().c_str(),
      thisNode.unparse_dash().c_str());
  
  inactive = true;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnSearch)
