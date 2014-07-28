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
#ifndef TOPOLOGY_INFO_HH
#define TOPOLOGY_INFO_HH

#include <click/element.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>
#include <click/sync.hh>

#include "elements/brn/brnelement.hh"
#include "topology_info_edge.hh"
#include "topology_info_node.hh"

CLICK_DECLS

class TopologyInfo :  public BRNElement
{
private:
    void nonthreadsafe_addBridge(const TopologyInfoEdge &bridge);
    void nonthreadsafe_addArticulationPoint(const TopologyInfoNode &ap);
    
public:   
    TopologyInfo();
    ~TopologyInfo();
    
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();
    
    const char *class_name() const { return "TopologyInfo";}
    const char *processing() const { return AGNOSTIC;}
    
    const char *port_count() const
    {
        return "0/0";
    }

    void incNoDetection();
    
    void addBridge(EtherAddress *a, EtherAddress *b, float probability=0.0);
    void removeBridge(EtherAddress *a, EtherAddress *b);
    void setBridges(Vector<TopologyInfoEdge*> &new_bridges);
    
    void addArticulationPoint(EtherAddress *a, float probability=0.0);
    void removeArticulationPoint(EtherAddress *a);
    void setArticulationPoints(Vector<TopologyInfoNode*> &new_artpoints);
    
    void reset();
    
    TopologyInfoEdge* getBridge(EtherAddress *a, EtherAddress *b);
    TopologyInfoNode *getArticulationPoint(EtherAddress *a);
    
    String topology_info(void);
    String topology_info(String extra_data);

protected:
    Vector<TopologyInfoEdge*> _bridges;
    Vector<TopologyInfoNode*> _artpoints;
    Vector<TopologyInfoEdge*> _non_bridges;
    Vector<TopologyInfoNode*> _non_artpoints;
    int number_of_detections;
    
    Spinlock lock;
};

CLICK_ENDDECLS
#endif
