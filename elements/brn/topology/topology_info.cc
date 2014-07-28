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
#include "topology_info.hh"

CLICK_DECLS


TopologyInfo::TopologyInfo() :
number_of_detections(0)
{
  BRNElement::init();
}

TopologyInfo::~TopologyInfo()
{
  reset();
}

void TopologyInfo::reset()
{
  for (Vector<TopologyInfoEdge*>::const_iterator it = _bridges.begin(); it != _bridges.end(); it++)
  {
    TopologyInfoEdge* elem = *it;
    delete(elem);
  }
  _bridges.clear();
  
  for (Vector<TopologyInfoNode*>::const_iterator it = _artpoints.begin(); it != _artpoints.end(); it++)
  {
    TopologyInfoNode* elem = *it;
    delete(elem);
  }
  _artpoints.clear();
}

int
TopologyInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
TopologyInfo::initialize(ErrorHandler *)
{
  return 0;
}

void 
TopologyInfo::incNoDetection()
{
  number_of_detections++;
}

void
TopologyInfo::addBridge(EtherAddress *a, EtherAddress *b, float probability)
{
  TopologyInfoEdge *br = getBridge(a, b);
  if (br == NULL)
    _bridges.push_back(new TopologyInfoEdge(a, b, probability));
  else
    br->incDetection();
}

void
TopologyInfo::addBridge(const TopologyInfoEdge &bridge)
{
  _bridges.push_back(new TopologyInfoEdge(bridge));
}

void 
TopologyInfo::setBridges(Vector<TopologyInfoEdge*>& new_bridges)
{
  for (Vector<TopologyInfoEdge*>::const_iterator it = new_bridges.begin(); it != new_bridges.end(); it++)
  {
    TopologyInfoEdge *br = *it;
    addBridge(*br);
  }
}

void
TopologyInfo::removeBridge(EtherAddress* a, EtherAddress* b)
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

TopologyInfoEdge*
TopologyInfo::getBridge(EtherAddress *a, EtherAddress *b)
{
  for (int i = 0; i < _bridges.size(); i++)
    if (_bridges[i]->equals(a, b)) 
      return _bridges[i];

  return NULL;
}

void
TopologyInfo::addArticulationPoint(EtherAddress *a, float probability)
{
  TopologyInfoNode *ap = getArticulationPoint(a);
  if (ap == NULL)
    _artpoints.push_back(new TopologyInfoNode(a, probability));
  else
    ap->incDetection();
}

void
TopologyInfo::addArticulationPoint(const TopologyInfoNode &ap)
{
  _artpoints.push_back(new TopologyInfoNode(ap));
}

void 
TopologyInfo::setArticulationPoints(Vector<TopologyInfoNode*>& new_artpoints)
{
  for (Vector<TopologyInfoNode*>::const_iterator it = new_artpoints.begin(); it != new_artpoints.end(); it++)
  {
    TopologyInfoNode *ap = *it;
    addArticulationPoint(*ap);
  }
}


void
TopologyInfo::removeArticulationPoint(EtherAddress* a)
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

TopologyInfoNode*
TopologyInfo::getArticulationPoint(EtherAddress *a)
{
  for (int i = 0; i < _artpoints.size(); i++)
  {
    if (_artpoints[i]->equals(a))
      return _artpoints[i];
  }

  return NULL;
}

/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

String
TopologyInfo::topology_info(void)
{
  return (topology_info(""));
}

String
TopologyInfo::topology_info(String extra_data)
{
  StringAccum sa;

  sa << "<topology_info ";
  sa << "node='" << BRN_NODE_NAME << "' ";
  sa << "time='" << Timestamp::now() << "' ";
  sa << "extra_data='" << extra_data << "' >\n";
  
  sa << "\t<bridges count='" << _bridges.size() << "' >\n";
  for (int i = 0; i < _bridges.size(); i++)
  {
    TopologyInfoEdge *br = _bridges[i];
    sa << "\t\t<bridge ";
    sa << "id='" << (i + 1) << "' ";
    sa << "time='" << br->time_of_last_detection.unparse() << "' ";
    sa << "node_a='" << br->node_a << "' ";
    sa << "node_b='" << br->node_b << "' ";
    sa << "/>\n";
  }
  sa << "\t</bridges>\n";

  sa << "\t<articulationpoints count='" << _artpoints.size() << "' >\n";
  for (int i = 0; i < _artpoints.size(); i++)
  {
    TopologyInfoNode *ap = _artpoints[i];
    sa << "\t\t<articulationpoint ";
    sa << "id='" << (i + 1) << "' ";
    sa << "time='" << ap->time_of_last_detection.unparse() << "' ";
    sa << "node='" << ap->node << "' ";
    sa << "/>\n";
  }
  sa << "\t</articulationpoints>\n";

  sa << "</topology_info>\n";

  return sa.take_string();
}

enum
{
  H_TOPOLOGY_INFO,
  H_ARTICULATION_POINT,
  H_BRIDGE
};

static String
read_param(Element *e, void *thunk)
{
  TopologyInfo *t_info = (TopologyInfo *) e;

  switch ((uintptr_t) thunk)
  {
  case H_TOPOLOGY_INFO: return ( t_info->topology_info());
  default: return String();
  }
}

static int
write_param(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  TopologyInfo *topoInfo = (TopologyInfo *) e;
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  switch ((intptr_t) vparam)
  {
  case H_ARTICULATION_POINT:
  {
    EtherAddress ap;
    cp_ethernet_address(args[0], &ap);

    topoInfo->addArticulationPoint(&ap);
    break;
  }
  case H_BRIDGE:
  {
    EtherAddress etherA, etherB;

    cp_ethernet_address(args[0], &etherA);
    cp_ethernet_address(args[1], &etherB);

    topoInfo->addBridge(&etherA, &etherB);
    break;
  }
  }

  return 0;
}

void
TopologyInfo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("topology_info", read_param, (void *) H_TOPOLOGY_INFO);

  add_write_handler("articulation_point", write_param, (void *) H_ARTICULATION_POINT);
  add_write_handler("bridge", write_param, (void *) H_BRIDGE);

}


CLICK_ENDDECLS
EXPORT_ELEMENT(TopologyInfo)
