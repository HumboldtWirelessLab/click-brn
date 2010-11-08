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

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "topology_info.hh"

CLICK_DECLS

TopologyInfo::TopologyInfo()
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

/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

String
TopologyInfo::topology_info(void)
{
  StringAccum sa;

  Bridge *br;
  ArticulationPoint *fp;

  sa << "Bridges (" << _bridges.size() << ") :\n";
  for( int i = 0; i < _bridges.size(); i++ )
  {
    br = _bridges[i];
    sa << i <<  "\t" << br->node_a << " <-> " << br->node_b << "\n";
  }

  sa << "\nArticulationPoint:\n";
  for( int i = 0; i < _artpoints.size(); i++ )
  {
    fp = _artpoints[i];
    sa << i <<  "\t" << fp->node << " <-> " << fp->node << "\n";
  }
  sa << "\n";

  return sa.take_string();
}

enum {
  H_TOPOLOGY_INFO
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

void
TopologyInfo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("topology_info", read_param , (void *)H_TOPOLOGY_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TopologyInfo)
