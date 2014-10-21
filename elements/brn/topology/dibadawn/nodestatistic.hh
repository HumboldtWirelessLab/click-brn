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

#ifndef TOPOLOGY_DIBADAWN_STATISTIC_HH
#define TOPOLOGY_DIBADAWN_STATISTIC_HH


#include <click/config.h>
#include <click/sync.hh>

#include "edgemarking.hh"
#include "topology_info_container.hh"
#include "config.hh"
#include "../topology_info.hh"


CLICK_DECLS;

class DibadawnNodeStatistic
{
public:
    DibadawnNodeStatistic(DibadawnConfig &cfg);
    
    double competenceByUsedHops(uint8_t hops);
    double weightByCompetence(double competence);
    void setTopologyInfo(TopologyInfo *topoInfo);
    
    bool isBridge(TopologyInfoEdge *edge);
    bool isBridgeByUnanimousRule(TopologyInfoEdge* edge);
    bool isBridgeByMajorityRule(TopologyInfoEdge* edgeA);
    bool isBridgeBySingleForRule(TopologyInfoEdge* edgeA);
    bool isBridgeByIntelligentMajorityRule(TopologyInfoEdge* edgeA);
    bool isBridgeByTrustedNoBridgeRule(TopologyInfoEdge* edgeA);
    bool isBridgeByWeightedRule(TopologyInfoEdge* edgeA);
    bool isBridgeByLastSet(TopologyInfoEdge* edgeA);
    
    double calcWeight(double p);
    
    bool isAP(TopologyInfoNode *node);
    bool isAPByUnanimousRule(TopologyInfoNode* node);
    bool isAPByMajorityRule(TopologyInfoNode* nodeA);
    bool isAPBySingleForRule(TopologyInfoNode* nodeA);
    bool isAPByIntelligentMajorityRule(TopologyInfoNode* nodeA);
    bool isAPByTrustedNoBridgeRule(TopologyInfoNode* nodeA);
    bool isAPByWeightedRule(TopologyInfoNode* nodeA);
    bool isAPByLastSet(TopologyInfoNode* nodeA);
    
    void printFinalResult(String extra_data);
    void printSearchResultSets(String extraData);
    void appendSearchResult(DibadawnTopologyInfoContainer &result);
    void updateTopologyInfoByVoting();
    
private:
    DibadawnConfig &config;
    
    Spinlock lock;
    Vector<DibadawnEdgeMarking> edgeMarkings;
    int maxMarkings;
    
    Vector<DibadawnTopologyInfoContainer*> searchResults;
    
    void getListOfKnowEdges(DibadawnTopologyInfoContainer &result);
    void getListOfKnowNodes(DibadawnTopologyInfoContainer &result);
    
    TopologyInfo *topologyInfo;
};

CLICK_ENDDECLS
#endif
