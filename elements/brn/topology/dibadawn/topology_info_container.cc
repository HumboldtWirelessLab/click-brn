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
  
  for (Vector<TopologyInfoEdge*>::const_iterator it = src._edges.begin(); it != src._edges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    TopologyInfoEdge *elm_copy = new TopologyInfoEdge(*elem);
    _edges.push_back(elm_copy);
  }
  
  for (Vector<TopologyInfoNode*>::const_iterator it = src._nodes.begin(); it != src._nodes.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    TopologyInfoNode *elm_copy = new TopologyInfoNode(*elem);
    _nodes.push_back(elm_copy);
  }
}

DibadawnTopologyInfoContainer::~DibadawnTopologyInfoContainer()
{
  for (Vector<TopologyInfoEdge*>::const_iterator it = _edges.begin(); it != _edges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    delete(elem);
  }
  _edges.clear();
  
  for (Vector<TopologyInfoNode*>::const_iterator it = _nodes.begin(); it != _nodes.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    delete(elem);
  }
  _nodes.clear();
}

void
DibadawnTopologyInfoContainer::addBridge(EtherAddress *a, EtherAddress *b, float probability)
{
  TopologyInfoEdge *br = getBridge(a, b);
  if (br == NULL)
  {
    TopologyInfoEdge * edge = new TopologyInfoEdge(a, b, probability);
    edge->setBridge();
    _edges.push_back(edge);
  }
  else
    br->incDetection();
}

void DibadawnTopologyInfoContainer::addEdge(TopologyInfoEdge &e)
{
  TopologyInfoEdge *br = getEdge(&(e.node_a), &(e.node_b));
  if (br == NULL)
  {
    TopologyInfoEdge * edge = new TopologyInfoEdge(e);
    _edges.push_back(edge);
  }
  else
    br->incDetection();
}

void
DibadawnTopologyInfoContainer::removeBridge(EtherAddress* a, EtherAddress* b)
{
  for (Vector<TopologyInfoEdge*>::iterator it = _edges.begin(); it != _edges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    if (elem->equals(a, b) && elem->isBridge())
    {
      _edges.erase(it);
      break; // After erasing an element, the iterator could not be used
    }
  }
}

void
DibadawnTopologyInfoContainer::addNonBridge(EtherAddress *a, EtherAddress *b, float probability)
{
  TopologyInfoEdge *nbr = getNonBridge(a, b);
  if (nbr == NULL) 
  {
    TopologyInfoEdge *edge = new TopologyInfoEdge(a, b, probability);
    edge->setNonBridge();
    _edges.push_back(edge);
  }
  else
    nbr->incDetection();
}

void
DibadawnTopologyInfoContainer::removeNonBridge(EtherAddress* a, EtherAddress* b)
{
  for (Vector<TopologyInfoEdge*>::iterator it = _edges.begin(); it != _edges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    if (elem->equals(a, b) && elem->isNonBridge())
    {
      _edges.erase(it);
      break; // After erasing an element, the iterator could not be used
    }
  }
}

void
DibadawnTopologyInfoContainer::addNode(TopologyInfoNode &n)
{
  TopologyInfoNode *ap = getNode(&(n.node));
  if (ap == NULL)
  {
    _nodes.push_back(new TopologyInfoNode(n));
  }
  else
    ap->incDetection();
}

void
DibadawnTopologyInfoContainer::addArticulationPoint(EtherAddress *a, float probability)
{
  TopologyInfoNode *ap = getArticulationPoint(a);
  if (ap == NULL)
  {
    TopologyInfoNode *node = new TopologyInfoNode(a, probability);
    node->setArticulationPoint();
    _nodes.push_back(node);
  }
  else
    ap->incDetection();
}

void
DibadawnTopologyInfoContainer::removeArticulationPoint(EtherAddress* a)
{
  for (Vector<TopologyInfoNode*>::iterator it = _nodes.begin(); it != _nodes.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    if (elem->equals(a) && elem->isArticulationPoint())
    {
      _nodes.erase(it);
      break; // After erasing an element, the iterator could not be used
    }
  }
}

void
DibadawnTopologyInfoContainer::addNonArticulationPoint(EtherAddress *a, float probability)
{
  TopologyInfoNode *nap = getNonArticulationPoint(a);
  if (nap == NULL)
  {
    TopologyInfoNode *node = new TopologyInfoNode(a, probability);
    node->setNonArticulationPoint();
    _nodes.push_back(node);
  }
  else
    nap->incDetection();
}

void
DibadawnTopologyInfoContainer::removeNonArticulationPoint(EtherAddress* a)
{
  for (Vector<TopologyInfoNode*>::iterator it = _nodes.begin(); it != _nodes.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    if (elem->equals(a) && elem->isNonArticulationPoint())
    {
      _nodes.erase(it);
      break; // After erasing an element, the iterator could not be used
    }
  }
}

TopologyInfoEdge*
DibadawnTopologyInfoContainer::getBridge(EtherAddress *a, EtherAddress *b)
{
  for (int i = 0; i < _edges.size(); i++)
    if (_edges[i]->equals(a, b) && _edges[i]->isBridge()) 
      return _edges[i];

  return NULL;
}

TopologyInfoEdge*
DibadawnTopologyInfoContainer::getEdge(EtherAddress *a, EtherAddress *b)
{
  for (int i = 0; i < _edges.size(); i++)
    if (_edges[i]->equals(a, b)) 
      return _edges[i];

  return NULL;
}

TopologyInfoEdge*
DibadawnTopologyInfoContainer::getNonBridge(EtherAddress *a, EtherAddress *b)
{
  for (int i = 0; i < _edges.size(); i++)
    if (_edges[i]->equals(a, b) && _edges[i]->isBridge()) 
      return _edges[i];

  return NULL;
}

TopologyInfoNode*
DibadawnTopologyInfoContainer::getNode(EtherAddress *a)
{
  for (int i = 0; i < _nodes.size(); i++)
  {
    if (_nodes[i]->equals(a))
      return _nodes[i];
  }

  return NULL;
}

TopologyInfoNode*
DibadawnTopologyInfoContainer::getArticulationPoint(EtherAddress *a)
{
  for (int i = 0; i < _nodes.size(); i++)
  {
    if (_nodes[i]->equals(a) && _nodes[i]->isArticulationPoint())
      return _nodes[i];
  }

  return NULL;
}

TopologyInfoNode*
DibadawnTopologyInfoContainer::getNonArticulationPoint(EtherAddress *a)
{
  for (int i = 0; i < _nodes.size(); i++)
  {
    if (_nodes[i]->equals(a) && _nodes[i]->isNonArticulationPoint())
      return _nodes[i];
  }

  return NULL;
}

bool DibadawnTopologyInfoContainer::containsEdge(TopologyInfoEdge* e)
{
  for (int i = 0; i < _edges.size(); i++)
    if (_edges[i]->equals(e)) 
      return true;
  
  return(false);
}

bool DibadawnTopologyInfoContainer::containsNode(TopologyInfoNode* e)
{
  for (int i = 0; i < _nodes.size(); i++)
    if (_nodes[i]->equals(e)) 
      return true;
  
  return(false);
}

void DibadawnTopologyInfoContainer::print(EtherAddress thisNode, String extraData)
{
  StringAccum sa;

  sa << "<DibadawnTopologyInfoContainer ";
  sa << "node='" << thisNode.unparse() << "' ";
  sa << "time='" << Timestamp::now() << "' ";
  sa << "extra_data='" << extraData << "' ";
  sa << ">\n";
  
  printContent(sa);
  
  sa << "</DibadawnTopologyInfoContainer>\n";
  
  click_chatter(sa.take_string().c_str());
}

void DibadawnTopologyInfoContainer::printContent(StringAccum& sa)
{
  for (int i = 0; i < _edges.size(); i++)
  {
    TopologyInfoEdge *e = _edges[i];
    sa << "\t<edge ";
    sa << "node_a='" << e->node_a << "' ";
    sa << "node_b='" << e->node_b << "' ";
    sa << "isBridge='" << e->isBridge() << "' ";
    sa << "isNonBridge='" << e->isNonBridge() << "' ";
    sa << "prob='" << e->probability << "' ";
    sa << "/>\n";
  }
  
  for (int i = 0; i < _nodes.size(); i++)
  {
    TopologyInfoNode *node = _nodes[i];
    sa << "\t<node ";
    sa << "node='" << node->node << "' ";
    sa << "isAP='" << node->isArticulationPoint() << "' ";
    sa << "isNonAP='" << node->isNonArticulationPoint() << "' ";
    sa << "prob='" << node->probability << "' ";
    sa << "/>\n";
  }
}



CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnTopologyInfoContainer)
