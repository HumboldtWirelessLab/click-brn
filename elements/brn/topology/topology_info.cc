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

TopologyInfo::TopologyInfo():
  number_of_detections(0)
{
  BRNElement::init();
}

TopologyInfo::~TopologyInfo()
{
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
TopologyInfo::addBridge(EtherAddress *a, EtherAddress *b)
{
  if ( getBridge(a,b) == NULL ) _bridges.push_back(new Bridge(a,b));
}

void
TopologyInfo::addArticulationPoint(EtherAddress *a)
{
  if ( getArticulationPoint(a) == NULL ) _artpoints.push_back(new ArticulationPoint(a));
}

TopologyInfo::Bridge*
TopologyInfo::getBridge(EtherAddress *a, EtherAddress *b)
{
  for( int i = 0; i < _bridges.size(); i++ )
    if ( _bridges[i]->equals(a,b) ) return _bridges[i];

  return NULL;
}

TopologyInfo::ArticulationPoint *
TopologyInfo::getArticulationPoint(EtherAddress *a)
{
  for( int i = 0; i < _artpoints.size(); i++ )
    if ( _artpoints[i]->equals(a) ) return _artpoints[i];

  return NULL;
}


/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

String
TopologyInfo::topology_info(void)
{
  StringAccum sa;

  Bridge *br;
  ArticulationPoint *fp;

  sa << "<topology_info node=\"" << BRN_NODE_NAME << "\" >\n";
  sa << "\t<bridges count=\"" << _bridges.size() << ">\n";
  for( int i = 0; i < _bridges.size(); i++ )
  {
    br = _bridges[i];
    sa << "\t\t<bridge id=\"" << (i+1) << "\" node_a=\"" << br->node_a << "\" node_b=\"" << br->node_b << "\" />\n";
  }
  sa << "\t</bridges>\n";

  sa << "\t<articulationpoints count=\"" << _artpoints.size() << "\" >\n";
  for( int i = 0; i < _artpoints.size(); i++ )
  {
    fp = _artpoints[i];
    sa << "\t\t<articulationpoint id=\"" << (i+1) <<  "\" node=\"" << fp->node << "\" />\n";
  }
  sa << "\t</articulationpoints>\n</topology_info>\n";

  return sa.take_string();
}

enum {
  H_TOPOLOGY_INFO,
  H_ARTICULATION_POINT,
  H_BRIDGE
};

static String
read_param(Element *e, void *thunk)
{
  TopologyInfo *t_info = (TopologyInfo *)e;

  switch ((uintptr_t) thunk)
  {
    case H_TOPOLOGY_INFO : return ( t_info->topology_info() );
    default: return String();
  }
}

static int
write_param(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  TopologyInfo *f = (TopologyInfo *)e;
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  //int result = -1;

  switch((intptr_t)vparam) {
    case H_ARTICULATION_POINT: {
      EtherAddress ap;
      cp_ethernet_address(args[0], &ap);

      f->addArticulationPoint(&ap);
      break;
    }
    case H_BRIDGE: {
      EtherAddress a,b;

      cp_ethernet_address(args[0], &a);
      cp_ethernet_address(args[1], &b);

      f->addBridge(&a, &b);
      break;
    }
  }

//  click_chatter("Result: %d",result);

  return 0;
}

void
TopologyInfo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("topology_info", read_param , (void *)H_TOPOLOGY_INFO);

  add_write_handler("articulation_point", write_param, (void *) H_ARTICULATION_POINT);
  add_write_handler("bridge", write_param, (void *) H_BRIDGE);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(TopologyInfo)
