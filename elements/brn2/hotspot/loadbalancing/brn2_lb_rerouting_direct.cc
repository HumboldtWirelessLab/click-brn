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
#include <click/confparse.hh>

#include "brn2_lb_rerouting_direct.hh"

CLICK_DECLS

LoadbalancingReroutingDirect::LoadbalancingReroutingDirect()
{
}

LoadbalancingReroutingDirect::~LoadbalancingReroutingDirect()
{
}

void *LoadbalancingReroutingDirect::cast(const char *name)
{
  if (strcmp(name, "LoadbalancingReroutingDirect") == 0)
    return (LoadbalancingReroutingDirect *) this;
  else if (strcmp(name, "LoadbalancingRerouting") == 0)
    return (LoadbalancingRerouting *) this;
  else
    return NULL;
}

int LoadbalancingReroutingDirect::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      cpEnd) < 0)
    return -1;

  return 0;
}

int LoadbalancingReroutingDirect::initialize(ErrorHandler *)
{
  return 0;
}

EtherAddress *
LoadbalancingReroutingDirect::getBestNodeForFlow()
{
  click_chatter("Direct");
  return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LoadbalancingReroutingDirect)
