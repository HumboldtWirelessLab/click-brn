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

#include "dibadawn.hh"


CLICK_DECLS;

DibadawnAlgorithm::DibadawnAlgorithm(BRNElement *click_element)
{
  brn_click_element = click_element;
}

void DibadawnAlgorithm::receive(DibadawnPacket& packet)
{
  if (packet.isInvalid())
  {
    click_chatter("<InvalidPacketRx node='%s' />", config.thisNodeAsCstr());
    return;
  }
  
  DibadawnSearch *search = getResponsibleSearch(packet);
  if (search == NULL)
  {
    search = new DibadawnSearch(brn_click_element, nodeStatistic, config, link_stat, packet.searchId);
    searches.push_back(search);
  }

  search->receive(packet);
}

DibadawnSearch* DibadawnAlgorithm::getResponsibleSearch(DibadawnPacket& packet)
{
  for (Vector<DibadawnSearch*>::iterator i = searches.begin(); i != searches.end(); i++)
  {
    DibadawnSearch *s = *i;
    if (s->isResponsibleFor(packet))
      return (s);
  }

  return (NULL);
}

void DibadawnAlgorithm::startNewSearch()
{
  DibadawnSearch *search = new DibadawnSearch(brn_click_element, nodeStatistic, config, link_stat);
  searches.push_back(search);
  search->start_search();
}

void DibadawnAlgorithm::setTopologyInfo(TopologyInfo* topoInfo)
{
  nodeStatistic.setTopologyInfo(topoInfo);
}

void DibadawnAlgorithm::resetLinkStat()
{
  link_stat.reset();
}

String DibadawnAlgorithm::getLinkStat()
{
  if(!config.useLinkStatistic)
    return(String("Link statistics of DIBADAWN isn't enabled by param USE_LINK_STAT"));
  
  StringAccum sa;
  sa << "<DibadawnLinkStat " ;
  sa << "node='" << config.thisNode.unparse_dash() << "' ";
  sa << "time='" << Timestamp::now().unparse() << "' >\n";
  
  sa << link_stat.asString();
  
  sa << "</DibadawnLinkStat>";
  
  return(sa.take_string());
}



CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnAlgorithm)
