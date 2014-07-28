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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "topology_info_container.hh"

CLICK_DECLS


DibadawnTopologyInfoContainer::DibadawnTopologyInfoContainer()
{
}

DibadawnTopologyInfoContainer::DibadawnTopologyInfoContainer(const DibadawnTopologyInfoContainer& src)
{
  for (Vector<TopologyInfoEdge*>::const_iterator it = src._non_bridges.begin(); it != src._non_bridges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    TopologyInfoEdge *elm_copy = new TopologyInfoEdge(*elem);
    _non_bridges.push_back(elm_copy);
  }
  
  for (Vector<TopologyInfoEdge*>::const_iterator it = src._bridges.begin(); it != src._bridges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    TopologyInfoEdge *elm_copy = new TopologyInfoEdge(*elem);
    _bridges.push_back(elm_copy);
  }
  
  for (Vector<TopologyInfoNode*>::const_iterator it = src._non_artpoints.begin(); it != src._non_artpoints.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    TopologyInfoNode *elm_copy = new TopologyInfoNode(*elem);
    _non_artpoints.push_back(elm_copy);
  }
  
  for (Vector<TopologyInfoNode*>::const_iterator it = src._artpoints.begin(); it != src._artpoints.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    TopologyInfoNode *elm_copy = new TopologyInfoNode(*elem);
    _artpoints.push_back(elm_copy);
  }
}

DibadawnTopologyInfoContainer::~DibadawnTopologyInfoContainer()
{
  for (Vector<TopologyInfoEdge*>::const_iterator it = _non_bridges.begin(); it != _non_bridges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    delete(elem);
  }
  _non_bridges.clear();
  
  for (Vector<TopologyInfoEdge*>::const_iterator it = _bridges.begin(); it != _bridges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    delete(elem);
  }
  _bridges.clear();
  
  for (Vector<TopologyInfoNode*>::const_iterator it = _non_artpoints.begin(); it != _non_artpoints.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    delete(elem);
  }
  _non_artpoints.clear();
  
  for (Vector<TopologyInfoNode*>::const_iterator it = _artpoints.begin(); it != _artpoints.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    delete(elem);
  }
  _artpoints.clear();
}

void
DibadawnTopologyInfoContainer::addBridge(EtherAddress *a, EtherAddress *b, float probability)
{
  TopologyInfoEdge *br = getBridge(a, b);
  if (br == NULL)
    _bridges.push_back(new TopologyInfoEdge(a, b, probability));
  else
    br->incDetection();
}

void
DibadawnTopologyInfoContainer::removeBridge(EtherAddress* a, EtherAddress* b)
{
  for (Vector<TopologyInfoEdge*>::iterator it = _bridges.begin(); it != _bridges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    if (elem->equals(a, b))
    {
      _bridges.erase(it);
      break; // After erasing an element, the iterator could not be used
    }
  }
}

void
DibadawnTopologyInfoContainer::addNonBridge(EtherAddress *a, EtherAddress *b, float probability)
{
  TopologyInfoEdge *nbr = getNonBridge(a, b);
  if (nbr == NULL)
    _non_bridges.push_back(new TopologyInfoEdge(a, b, probability));
  else
    nbr->incDetection();
}

void
DibadawnTopologyInfoContainer::removeNonBridge(EtherAddress* a, EtherAddress* b)
{
  for (Vector<TopologyInfoEdge*>::iterator it = _non_bridges.begin(); it != _non_bridges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    if (elem->equals(a, b))
    {
      _bridges.erase(it);
      break; // After erasing an element, the iterator could not be used
    }
  }
}

void
DibadawnTopologyInfoContainer::addArticulationPoint(EtherAddress *a, float probability)
{
  TopologyInfoNode *ap = getArticulationPoint(a);
  if (ap == NULL)
    _artpoints.push_back(new TopologyInfoNode(a, probability));
  else
    ap->incDetection();
}

void
DibadawnTopologyInfoContainer::removeArticulationPoint(EtherAddress* a)
{
  for (Vector<TopologyInfoNode*>::iterator it = _artpoints.begin(); it != _artpoints.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    if (elem->equals(a))
    {
      _artpoints.erase(it);
      break; // After erasing an element, the iterator could not be used
    }
  }
}

void
DibadawnTopologyInfoContainer::addNonArticulationPoint(EtherAddress *a, float probability)
{
  TopologyInfoNode *nap = getNonArticulationPoint(a);
  if (nap == NULL)
    _non_artpoints.push_back(new TopologyInfoNode(a, probability));
  else
    nap->incDetection();
}

void
DibadawnTopologyInfoContainer::removeNonArticulationPoint(EtherAddress* a)
{
  for (Vector<TopologyInfoNode*>::iterator it = _non_artpoints.begin(); it != _non_artpoints.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    if (elem->equals(a))
    {
      _non_artpoints.erase(it);
      break; // After erasing an element, the iterator could not be used
    }
  }
}


TopologyInfoEdge*
DibadawnTopologyInfoContainer::getBridge(EtherAddress *a, EtherAddress *b)
{
  for (int i = 0; i < _bridges.size(); i++)
    if (_bridges[i]->equals(a, b)) 
      return _bridges[i];

  return NULL;
}

TopologyInfoEdge*
DibadawnTopologyInfoContainer::getNonBridge(EtherAddress *a, EtherAddress *b)
{
  for (int i = 0; i < _non_bridges.size(); i++)
    if (_non_bridges[i]->equals(a, b)) 
      return _non_bridges[i];

  return NULL;
}

TopologyInfoNode*
DibadawnTopologyInfoContainer::getArticulationPoint(EtherAddress *a)
{
  for (int i = 0; i < _artpoints.size(); i++)
  {
    if (_artpoints[i]->equals(a))
      return _artpoints[i];
  }

  return NULL;
}

TopologyInfoNode*
DibadawnTopologyInfoContainer::getNonArticulationPoint(EtherAddress *a)
{
  for (int i = 0; i < _non_artpoints.size(); i++)
  {
    if (_non_artpoints[i]->equals(a))
      return _non_artpoints[i];
  }

  return NULL;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnTopologyInfoContainer)
