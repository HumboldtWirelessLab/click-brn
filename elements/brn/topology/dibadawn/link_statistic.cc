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
#include "elements/grid/ackresponder.hh"
#include "link_statistic.hh"

CLICK_DECLS


DibadawnLinkStatistic::DibadawnLinkStatistic()
:   numTx(0)
{
  click_chatter("DibadawnLinkStatistic");
}

void DibadawnLinkStatistic::increaseTxCounter()
{
  numTx++;
}

void DibadawnLinkStatistic::increaseRxCounter(EtherAddress& addr)
{
  NeighborLink *link = getNeiborEntry(addr);
  link->numRx++;
}

DibadawnLinkStatistic::NeighborLink* DibadawnLinkStatistic::getNeiborEntry(EtherAddress& addr)
{
  for(Vector<NeighborLink*>::iterator it = links.begin(); it != links.end(); it++)
  {
    NeighborLink *link = *it;
    if(link->addr == addr)
      return(link);
  }
  
  DibadawnLinkStatistic::NeighborLink *new_link = new DibadawnLinkStatistic::NeighborLink(addr);
  links.push_back(new_link);
  
  return(new_link);
}

void DibadawnLinkStatistic::reset()
{
  numTx = 0;  
  
  for(Vector<NeighborLink*>::iterator it = links.begin(); it != links.end(); it++)
  {
    NeighborLink *link = *it;
    delete(link);
  }
  links.clear();
}

String DibadawnLinkStatistic::asString()
{
  StringAccum sa;
  sa << "  <tx num='" << numTx << "' />\n";
  
  for(Vector<NeighborLink*>::iterator it = links.begin(); it != links.end(); it++)
  {
    NeighborLink *link = *it;
    sa << "  <rx node='" << link->addr.unparse() << "' "<<"num='" << link->numRx << "' />\n";
  }
  
  return(sa.take_string());
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnLinkStatistic)
