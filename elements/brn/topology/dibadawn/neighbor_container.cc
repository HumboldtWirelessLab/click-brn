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
#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/type_traits.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "neighbor_container.hh"

CLICK_DECLS

size_t DibadawnNeighborContainer::numOfNeighbors()
{
  return (neighbors.size());
}

DibadawnNeighbor& DibadawnNeighborContainer::getNeighbor(EtherAddress& addr)
{
  for (int i = 0; i < neighbors.size(); i++)
  {
    DibadawnNeighbor &neighbor = neighbors.at(i);
    if (addr == neighbor.address)
      return (neighbor);
  }

  int idx = neighbors.size();
  neighbors.push_back(DibadawnNeighbor(addr));
  return (neighbors.at(idx));
}

DibadawnNeighbor& DibadawnNeighborContainer::getNeighbor(int num)
{
  return (neighbors.at(num));
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnNeighborContainer)
