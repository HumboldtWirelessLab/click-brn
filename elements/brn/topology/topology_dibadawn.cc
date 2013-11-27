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

#define LOG BrnLogger(__FILE__, __LINE__, NULL).info

CLICK_DECLS;


DibadawnSearch::DibadawnSearch(BRNElement *click_element, EtherAddress *addrOfThisNode)
{
  brn_click_element = click_element;
  thisNode = *addrOfThisNode;
  
  ideaOfPacket.searchId = DibadawnSearchId(Timestamp::now(), thisNode);
  ideaOfPacket.forwardedBy = thisNode;
  ideaOfPacket.isForward = true;
  ideaOfPacket.ttl = 255;  // TODO: Does this makes sense?
  isForwared = false;
  
  initTimer();
}

DibadawnSearch::DibadawnSearch(BRNElement *click_element, EtherAddress *addrOfThisNode, DibadawnPacket &packet)
{
  brn_click_element = click_element;
  thisNode = *addrOfThisNode;
  
  ideaOfPacket = packet;
  ideaOfPacket.forwardedBy = thisNode;
  isForwared = false;
  
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

  /* TODO Change this value */
  forwardTimer->schedule_after_msec(100);
}

String DibadawnSearch::asString()
{
  return(ideaOfPacket.searchId.AsString());
}

void DibadawnSearch::start_search()
{
  sendPerBroadcastWithTimeout();
  isForwared = true;
}

void DibadawnSearch::sendPerBroadcastWithTimeout()
{
  ideaOfPacket.log("DibadawnPacketTx");
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
  receivedPacket.log("DibadawnPacketRx");
  if(receivedPacket.isForward)
  {
    receiveForwardMessage(receivedPacket);
  }
  else
    LOG("<--! Not Implemented yet -->");
}

void DibadawnSearch::receiveForwardMessage(DibadawnPacket &receivedPacket)
{
  if (isForwared)
  {
    LOG("<--! Already forwared, TODO: ... -->");
  }
  else
  {
    ideaOfPacket.ttl = receivedPacket.ttl - 1;
    ideaOfPacket.treeParent = receivedPacket.forwardedBy;
    sendPerBroadcastWithTimeout();
    isForwared = true;
  }
}

void DibadawnSearch::forwardTimeout()
{
  LOG("<ForwardTimeout searchId='%s' node='%s' />",
      ideaOfPacket.searchId.AsString().c_str(),
      thisNode.unparse_dash().c_str());
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnSearch)
