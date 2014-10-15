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
#ifndef DIBADAWN_TOPOLOGY_INFO_HH
#define DIBADAWN_TOPOLOGY_INFO_HH

#include <click/element.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>

#include "elements/brn/brnelement.hh"
#include "../topology_info_edge.hh"
#include "../topology_info_node.hh"

CLICK_DECLS

class DibadawnTopologyInfoContainer
{
public:    
    DibadawnTopologyInfoContainer();
    DibadawnTopologyInfoContainer(const DibadawnTopologyInfoContainer &src);
    ~DibadawnTopologyInfoContainer();
    
    void addBridge(EtherAddress *a, EtherAddress *b, float probability=0.0);
    void addEdge(TopologyInfoEdge &e);
    void removeBridge(EtherAddress *a, EtherAddress *b);
    void addNonBridge(EtherAddress *a, EtherAddress *b, float probability=0.0);
    void removeNonBridge(EtherAddress *a, EtherAddress *b);
    void addArticulationPoint(EtherAddress *a, float probability=0.0);
    void removeArticulationPoint(EtherAddress *a);
    void addNonArticulationPoint(EtherAddress *a, float probability=0.0);
    void removeNonArticulationPoint(EtherAddress *a);
    
    TopologyInfoEdge* getBridge(EtherAddress *a, EtherAddress *b);
    TopologyInfoEdge* getEdge(EtherAddress *a, EtherAddress *b);
    TopologyInfoEdge* getNonBridge(EtherAddress *a, EtherAddress *b);
    TopologyInfoNode *getArticulationPoint(EtherAddress *a);
    TopologyInfoNode *getNonArticulationPoint(EtherAddress *a);
    bool containsEdge(TopologyInfoEdge*);
    void print(EtherAddress thisNode, String extraData);
    void printContent(StringAccum &sa);

    Vector<TopologyInfoEdge*> _edges;
    Vector<TopologyInfoNode*> _nodes;
};

CLICK_ENDDECLS
#endif
