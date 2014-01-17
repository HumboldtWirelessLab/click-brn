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


CLICK_DECLS;

class DibadawnStatistic
{
public:
    void updateEdgeMarking(DibadawnEdgeMarking &marking);
    DibadawnStatistic();

private:
    Spinlock lock;
    Vector<DibadawnEdgeMarking> edgeMarkings;
    int maxMarkings;
};

CLICK_ENDDECLS
#endif
