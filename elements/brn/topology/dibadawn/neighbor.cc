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
#include "neighbor.hh"
#include "dibadawn_packet.hh"

CLICK_DECLS

DibadawnNeighbor::DibadawnNeighbor(EtherAddress& addr, DibadawnSearchId &id) :
  address(addr),
  searchId(id)
{
}

bool DibadawnNeighbor::hasNonEmptyPairIntersection(DibadawnNeighbor& other)
{
  for (int i = 0; i < messages.size(); i++)
  {
    DibadawnPayloadElement& elemA = messages.at(i);
    for (int j = 0; j < other.messages.size(); j++)
    {
      DibadawnPayloadElement& elemB = other.messages.at(j);

      if (elemA == elemB)
        return (true);
    }
  }
  return (false);
}

void DibadawnNeighbor::printIntersection(EtherAddress &thisNode, DibadawnNeighbor& other)
{
  StringAccum sa;
  sa << "<Intersection ";
  sa << "node='" << thisNode.unparse_dash() << "' ";
  sa << "time='" << Timestamp::now().unparse() << "' ";
  sa << "searchId='" << searchId << "' ";
  sa << "nodeA='" << address.unparse() << "' ";
  sa << "nodeB='" << other.address.unparse() << "' ";
  sa << ">\n";
  
  for (int i = 0; i < messages.size(); i++)
  {
    DibadawnPayloadElement& elemA = messages.at(i);
    for (int j = 0; j < other.messages.size(); j++)
    {
      DibadawnPayloadElement& elemB = other.messages.at(j);

      if (elemA == elemB)
      {
        sa << "  " << elemA << "\n";
      }
    }
  }

  sa << "</Intersection>";
  click_chatter(sa.take_string().c_str());
}

void DibadawnNeighbor::copyPayloadIfNecessary(DibadawnPayloadElement& src)
{
  if (!hasSameCycle(src))
    messages.push_back(src);
}

bool DibadawnNeighbor::hasSameCycle(DibadawnPayloadElement& elem)
{
  for (int i = 0; i < messages.size(); i++)
  {
    DibadawnPayloadElement& elem2 = messages.at(i);
    if (elem2.isBridge)
      continue;

    if (elem.cycle == elem2.cycle)
      return (true);
  }
  return (false);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnNeighbor)
