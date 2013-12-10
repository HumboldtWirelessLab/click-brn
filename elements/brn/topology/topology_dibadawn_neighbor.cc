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
#include "topology_dibadawn_neighbor.hh"
#include "topology_dibadawn_packet.hh"


CLICK_DECLS

DibadawnNeighbor::DibadawnNeighbor(EtherAddress& addr)
{
  address = addr;
}

bool DibadawnNeighbor::hasNonEmptyIntersection(DibadawnNeighbor& other)
{
  for(int i=0; i<messages.size(); i++)
  {
    for(int j=0; j<other.messages.size(); j++)
    {
      DibadawnPacket& m1 = messages.at(i);
      DibadawnPacket& m2 = other.messages.at(j);
      
      if(m1.hasNonEmptyIntersection(m2))
        return(true);
    }
  }
  return(false);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnNeighbor)
