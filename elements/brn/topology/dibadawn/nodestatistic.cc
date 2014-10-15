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
#include "config.hh"


CLICK_DECLS

DibadawnNodeStatistic::DibadawnNodeStatistic(DibadawnConfig &cfg):
config(cfg)
{
  maxMarkings = 3;
  topologyInfo = NULL;
}

bool DibadawnNodeStatistic::isBridge(TopologyInfoEdge* edge)
{
  switch(config.votingRule)
  {
  case 0: return(isBridgeByUnanimousRule(edge));

  case 1: return(isBridgeByMajorityRule(edge));

  case 2: return(isBridgeBySingleForRule(edge));

  case 3: return(isBridgeByIntelligentMajorityRule(edge));

  case 4: return(isBridgeByTrustedNoBridgeRule(edge));

  case 5: return(isBridgeByWeightedRule(edge));

  default: return(isBridgeByLastSet(edge));
  }
  
  return(false);
}

bool DibadawnNodeStatistic::isBridgeByUnanimousRule(TopologyInfoEdge* edgeA)
{
  for (Vector<DibadawnTopologyInfoContainer*>::const_iterator it = searchResults.begin(); it != searchResults.end(); it++)
  {
    DibadawnTopologyInfoContainer *container = *it;
    
    for (Vector<TopologyInfoEdge*>::const_iterator it2 = container->_edges.begin(); it2 !=  container->_edges.end(); it2++)
    {
      TopologyInfoEdge *edgeB = *it2;
      
      if(edgeA->equals(edgeB) && edgeB->isNonBridge())
        return(false);
    }
  }
  
  return (true);
}

bool DibadawnNodeStatistic::isBridgeByMajorityRule(TopologyInfoEdge* edgeA)
{
  size_t numBridges = 0;
  size_t numNoBrigdes = 0;

  for (Vector<DibadawnTopologyInfoContainer*>::const_iterator it = searchResults.begin(); it != searchResults.end(); it++)
  {
    DibadawnTopologyInfoContainer *container = *it;
    
    for (Vector<TopologyInfoEdge*>::const_iterator it2 = container->_edges.begin(); it2 !=  container->_edges.end(); it2++)
    {
      TopologyInfoEdge *edgeB = *it2;
      
      if(edgeA->equals(edgeB))
      {
        if(edgeB->isBridge())
          numBridges++;
        else if(edgeB->isNonBridge())
          numNoBrigdes++;
      }
    }
  }

  return (numBridges > numNoBrigdes);
}

bool DibadawnNodeStatistic::isBridgeBySingleForRule(TopologyInfoEdge* edgeA)
{
  for (Vector<DibadawnTopologyInfoContainer*>::const_iterator it = searchResults.begin(); it != searchResults.end(); it++)
  {
    DibadawnTopologyInfoContainer *container = *it;
    
    for (Vector<TopologyInfoEdge*>::const_iterator it2 = container->_edges.begin(); it2 !=  container->_edges.end(); it2++)
    {
      TopologyInfoEdge *edgeB = *it2;
      
      if(edgeA->equals(edgeB) && edgeB->isBridge())
        return(true);
    }
  }

  return (false);
}

bool DibadawnNodeStatistic::isBridgeByIntelligentMajorityRule(TopologyInfoEdge* edgeA)
{
  size_t numBridges = 0;
  size_t numNoBrigdes = 0;
  bool noBridgeHasTrustedVote = false;

  for (Vector<DibadawnTopologyInfoContainer*>::const_iterator it = searchResults.begin(); it != searchResults.end(); it++)
  {
    DibadawnTopologyInfoContainer *container = *it;
    
    for (Vector<TopologyInfoEdge*>::const_iterator it2 = container->_edges.begin(); it2 !=  container->_edges.end(); it2++)
    {
      TopologyInfoEdge *edgeB = *it2;
      
      if(edgeA->equals(edgeB))
      {
        if(edgeB->isBridge())
          numBridges++;
        else if(edgeB->isNonBridge())
        {
          numNoBrigdes++;
          if(edgeB->probability > config.trustThreshold)
            noBridgeHasTrustedVote = true;
        }
      }
    }
  }

  bool result;
  if(noBridgeHasTrustedVote)
    result = false;
  else
    result = numBridges > numNoBrigdes;
  
  return (result);
}

bool DibadawnNodeStatistic::isBridgeByTrustedNoBridgeRule(TopologyInfoEdge * edgeA)
{
  bool noBridgeHasTrustedVote = false;

  for (Vector<DibadawnTopologyInfoContainer*>::const_iterator it = searchResults.begin(); it != searchResults.end(); it++)
  {
    DibadawnTopologyInfoContainer *container = *it;
    
    for (Vector<TopologyInfoEdge*>::const_iterator it2 = container->_edges.begin(); it2 !=  container->_edges.end(); it2++)
    {
      TopologyInfoEdge *edgeB = *it2;
      
      if(edgeA->equals(edgeB))
      {
        if(edgeB->isNonBridge())
        {
          if(edgeB->probability > config.trustThreshold)
            noBridgeHasTrustedVote = true;
        }
      }
    }
  }

  bool result = true;
  if (noBridgeHasTrustedVote)
    result = false;
  return (result);
}

bool DibadawnNodeStatistic::isBridgeByWeightedRule(TopologyInfoEdge * edgeA)
{
  double sumBridges = 0;
  double sumNoBridges = 0;

  for (Vector<DibadawnTopologyInfoContainer*>::const_iterator it = searchResults.begin(); it != searchResults.end(); it++)
  {
    DibadawnTopologyInfoContainer *container = *it;

    for (Vector<TopologyInfoEdge*>::const_iterator it2 = container->_edges.begin(); it2 != container->_edges.end(); it2++)
    {
      TopologyInfoEdge *edgeB = *it2;

      if (edgeA->equals(edgeB))
      {
        if (edgeB->isBridge())
          sumBridges += calcWeight(edgeB->probability);
        else if (edgeB->isNonBridge())
          sumNoBridges += calcWeight(edgeB->probability);
      }
    }
  }
  
  return(sumBridges - sumNoBridges > calcWeight(config.probabilityForABridgeAtNet));
}

double DibadawnNodeStatistic::calcWeight(double p)
{
  return (log(p / (1 - p)));
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

bool DibadawnNodeStatistic::isBridgeByLastSet(TopologyInfoEdge* edgeA)
{
  DibadawnTopologyInfoContainer *container = searchResults.back();

  for (Vector<TopologyInfoEdge*>::const_iterator it2 = container->_edges.begin(); it2 != container->_edges.end(); it2++)
  {
    TopologyInfoEdge *edgeB = *it2;

    if (edgeA->equals(edgeB) && edgeB->isBridge())
      return (true);
  }

  return (false);
}


double DibadawnNodeStatistic::weightByCompetence(double competence)
{
  return( log(competence / (1-competence)) );
}

void DibadawnNodeStatistic::setTopologyInfo(TopologyInfo* topoInfo)
{
  topologyInfo = topoInfo;
}

void DibadawnNodeStatistic::printFinalResult(String extra_data)
{
  StringAccum sa;
  
  if(topologyInfo != NULL)
  {
    sa << topologyInfo->topology_info(extra_data);
    click_chatter(sa.take_string().c_str());
  }
}

void DibadawnNodeStatistic::printSearchResultSets(String extraData)
{
  StringAccum sa;
  
  sa << "<DibadawnSearchResultSets ";
  sa << "node='" << config.thisNode.unparse() << "' ";
  sa << "time='" << Timestamp::now() << "' ";
  sa << "extra_data='" << extraData << "' ";
  sa << ">\n";
  
  int i = 1;
  for (Vector<DibadawnTopologyInfoContainer*>::const_iterator it = searchResults.begin(); it != searchResults.end(); it++)
  {
    DibadawnTopologyInfoContainer *container = *it;
    
    sa << "<set " << "id='" << i++ << "' " << ">\n";
    container->printContent(sa);
    sa << "</set>\n";
  }
  
  sa << "</DibadawnSearchResultSets>\n";
    
  click_chatter(sa.take_string().c_str());
}

void DibadawnNodeStatistic::appendSearchResult(DibadawnTopologyInfoContainer& result)
{
  lock.acquire();
  
  if(searchResults.size() >= config.numOfVotingSets)
  {
    DibadawnTopologyInfoContainer *ti = *searchResults.begin();
    searchResults.pop_front();
    delete(ti);
  }
  
  DibadawnTopologyInfoContainer *container = new DibadawnTopologyInfoContainer(result);
  searchResults.push_back(container);
  
  lock.release();
}

//todo put the whole vector to tpinfo instead of resetting it first
void DibadawnNodeStatistic::updateTopologyInfoByVoting()
{
  DibadawnTopologyInfoContainer result;
  DibadawnTopologyInfoContainer edgeList;
  lock.acquire();
  
  printSearchResultSets("");
  
  getListOfKnowEdges(edgeList);
  edgeList.print(config.thisNode, "uniqie list");
  
   for (Vector<TopologyInfoEdge*>::const_iterator it = edgeList._edges.begin(); it !=  edgeList._edges.end(); it++)
   {
     TopologyInfoEdge *edge = *it;
     
     if(isBridge(edge))
       result.addBridge(&(edge->node_a), &(edge->node_b), edge->probability);
   }
  
  topologyInfo->reset();
  topologyInfo->setBridges(result._edges);
  topologyInfo->setArticulationPoints(result._nodes);
  
  lock.release();
}

void DibadawnNodeStatistic::getListOfKnowEdges(DibadawnTopologyInfoContainer &result)
{
 for (Vector<DibadawnTopologyInfoContainer*>::const_iterator it = searchResults.begin(); it != searchResults.end(); it++)
  {
    DibadawnTopologyInfoContainer *container = *it;
    
    for (Vector<TopologyInfoEdge*>::const_iterator it2 = container->_edges.begin(); it2 !=  container->_edges.end(); it2++)
    {
      TopologyInfoEdge *edge = *it2;
      if(result.containsEdge(edge))
        continue;
      result.addEdge(*edge);
    }
  }
}



CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnNodeStatistic)
