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

#ifndef TOPOLOGY_DIBADAWN_CYCLE_HH
#define TOPOLOGY_DIBADAWN_CYCLE_HH

#include <click/element.hh>

#include "searchid.hh"


CLICK_DECLS;

class DibadawnCycle {
public:
    static const size_t length = 10 + 6 + 6;
    String AsString();

    DibadawnCycle();
    DibadawnCycle(DibadawnSearchId &id, EtherAddress &addr1, EtherAddress &addr2);
    bool operator== (DibadawnCycle &b);
    void setData(const uint8_t *p);
    uint8_t* getData();
    
private:
    uint8_t contentAsBytes[length];
};

CLICK_ENDDECLS
#endif
