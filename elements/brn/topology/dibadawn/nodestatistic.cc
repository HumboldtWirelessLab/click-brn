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

#include <math.h>
#include "nodestatistic.hh"
#include "elements/brn/services/dhcp/dhcp.h"


CLICK_DECLS

DibadawnNodeStatistic::DibadawnNodeStatistic()
{
  maxMarkings = 3;
  topologyInfo = NULL;
}

void DibadawnNodeStatistic::updateEdgeMarking(DibadawnEdgeMarking &marking)
{
  lock.acquire();

  edgeMarkings.push_front(marking);
  if (edgeMarkings.size() > maxMarkings)
    edgeMarkings.pop_back();

  click_chatter("<DEBUG  objectAddr='%d' />", this);
  
  if(topologyInfo != NULL)
    topologyInfo->addBridge(&marking.nodeA, &marking.nodeB);
  
  lock.release();
}

bool DibadawnNodeStatistic::isBridgeByUnanimousRule()
{
  bool result = true;

  lock.acquire();

  for (int i = 0; i < edgeMarkings.size(); i++)
  {
    DibadawnEdgeMarking &marking = edgeMarkings.at(i);
    if (!marking.isBridge)
    {
      result = false;
      break;
    }
  }

  lock.release();

  return (result);
}

bool DibadawnNodeStatistic::isBridgeByMajorityRule()
{
  size_t numBridges = 0;
  size_t numNoBrigdes = 0;

  lock.acquire();

  for (int i = 0; i < edgeMarkings.size(); i++)
  {
    DibadawnEdgeMarking &marking = edgeMarkings.at(i);
    if (marking.isBridge)
      numBridges++;
    else
      numNoBrigdes++;
  }

  lock.release();

  return (numBridges > numNoBrigdes);
}

bool DibadawnNodeStatistic::isBridgeBySingleForRule()
{
  bool result = false;

  lock.acquire();

  for (int i = 0; i < edgeMarkings.size(); i++)
  {
    DibadawnEdgeMarking &marking = edgeMarkings.at(i);
    if (marking.isBridge)
    {
      result = true;
      break;
    }
  }

  lock.release();

  return (result);
}

bool DibadawnNodeStatistic::isBridgeByIntelligentMajorityRule()
{
  size_t numBridges = 0;
  size_t numNoBrigdes = 0;
  bool trustedVoteExists = false;

  lock.acquire();

  for (int i = 0; i < edgeMarkings.size(); i++)
  {
    DibadawnEdgeMarking &marking = edgeMarkings.at(i);
    if (marking.isTrusted)
      trustedVoteExists = true;
  }

  lock.release();

  bool result;
  if(trustedVoteExists)
    result = false;
  else
    result = numBridges > numNoBrigdes;
  
  return (result);
}

bool DibadawnNodeStatistic::isBridgeByTrustedNoBridgeRule()
{
  bool result = true;

  lock.acquire();

  for (int i = 0; i < edgeMarkings.size(); i++)
  {
    DibadawnEdgeMarking &marking = edgeMarkings.at(i);
    if (!marking.isBridge && marking.isTrusted)
      result = false;
  }

  lock.release();

  return (result);
}

bool DibadawnNodeStatistic::isBridgeByWeightedRule()
{
  return(false);
}

/*
 * These values are calculated by 1 - exp(x / 5 - 6) to get values close to the 
 * empirically derived values, used by bratislav milic.
 * 
 */
double DibadawnNodeStatistic::competenceByUsedHops(uint8_t hops)
{
  double result;
  
  switch (hops) {
  case 1: result = 0.95;
    break;
  case 2: result = 0.90;
    break;
  case 3: result = 0.9954834;
    break;
  case 4: result = 0.9944834;
    break;
  case 5: result = 0.9932621;
    break;
  case 6: result = 0.9917703;
    break;
  case 7: result = 0.9899482;
    break;
  case 8: result = 0.9877227;
    break;
  case 9: result = 0.9850044;
    break;
  case 10: result = 0.9816844;
    break;
  case 11: result = 0.9776292;
    break;
  case 12: result = 0.9726763;
    break;
  case 13: result = 0.9666267;
    break;
  case 14: result = 0.9592378;
    break;
  case 15: result = 0.9502129;
    break;
  case 16: result = 0.9391899;
    break;
  case 17: result = 0.9257264;
    break;
  case 18: result = 0.9092820;
    break;
  case 19: result = 0.8891968;
    break;
  case 20: result = 0.8646647;
    break;
  default: result = 0.85;
  }

  return (result);
}

double DibadawnNodeStatistic::weightByCompetence(double competence)
{
  return( log(competence / (1-competence)) );
}

void DibadawnNodeStatistic::setTopologyInfo(TopologyInfo* topoInfo)
{
  topologyInfo = topoInfo;
}

void DibadawnNodeStatistic::upateArticulationPoint(const EtherAddress &node, bool isArticulationPoint)
{
  if(isArticulationPoint)
     topologyInfo->addArticulationPoint(&node);
  else
    topologyInfo->removeArticulationPoint(&node);
}



CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnNodeStatistic)
