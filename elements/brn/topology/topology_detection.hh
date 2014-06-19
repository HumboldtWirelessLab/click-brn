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

#ifndef TOPOLOGY_DETECTION_HH
#define TOPOLOGY_DETECTION_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include <clicknet/ether.h>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "topology_info.hh"
#include "dibadawn/dibadawn.hh"

CLICK_DECLS

/*
 *=c
 *TopologyDetection()
 *=s
 */
class TopologyDetection : public BRNElement
{
public:

    TopologyDetection();
    ~TopologyDetection();

    const char *class_name()    const  { return "TopologyDetection"; }
    const char *port_count()    const  { return "1/1"; }
    const char *processing()    const  { return AGNOSTIC; }
    bool can_live_reconfigure() const  { return false; }
    
    int configure(Vector<String> &, ErrorHandler *);
    int reconfigure(String &, ErrorHandler *);
    
    int initialize(ErrorHandler *);
    void add_handlers();
    void push(int port, Packet *packet);
    void start_detection();
    String local_topology_info(void);
    String config();

private:
    uint32_t detection_id;
    DibadawnAlgorithm dibadawnAlgo;

    Brn2LinkTable *_lt;
    BRN2NodeIdentity *_node_identity;
    TopologyInfo *_topoInfo;

    void handle_detection(Packet *brn_packet);
};

CLICK_ENDDECLS
#endif
