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
#ifndef TOPOLOGY_INFO_EDGE_HH
#define TOPOLOGY_INFO_EDGE_HH

#include <click/element.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>


CLICK_DECLS


class TopologyInfoEdge 
{
public:
    EtherAddress node_a;
    EtherAddress node_b;
    uint32_t countDetection;
    Timestamp time_of_last_detection;
    enum 
    {
      eBridge, eNonBridge, eUnknown
    } bridgeState;
    float probability;

    TopologyInfoEdge(EtherAddress *a, EtherAddress *b, float probability = 0.0);
    void incDetection();
    bool equals(EtherAddress *a, EtherAddress *b);
    bool equals(TopologyInfoEdge *e);
    void setBridge();
    void setNonBridge();
    bool isBridge();
    bool isNonBridge();
};


CLICK_ENDDECLS
#endif
