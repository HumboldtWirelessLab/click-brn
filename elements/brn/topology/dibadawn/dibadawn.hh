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

#ifndef TOPOLOGY_DIBADAWN_HH
#define TOPOLOGY_DIBADAWN_HH

#include <click/element.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "search.hh"
#include "dibadawn_packet.hh"
#include "statistic.hh"


CLICK_DECLS;

class DibadawnAlgorithm
{
public:
    EtherAddress thisNode;
    BRNElement *brn_click_element;
    Vector<DibadawnSearch*> searches;

    DibadawnSearch* getResponsibleSearch(DibadawnPacket &packet);

public:
    DibadawnStatistic commonStatistic;

    DibadawnAlgorithm();
    DibadawnAlgorithm(BRNElement *brn_click_element, const EtherAddress &addrOfThisNode);
    void receive(DibadawnPacket &packet);
    void startNewSearch();

};

CLICK_ENDDECLS
#endif
