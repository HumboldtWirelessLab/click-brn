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

#include "topology_info_node.hh"

CLICK_DECLS


TopologyInfoNode::TopologyInfoNode(EtherAddress *n, float p)
:   time_of_last_detection(Timestamp::now()),
    probability(p)
{
  node = EtherAddress(n->data());
  countDetection = 1;
}

void TopologyInfoNode::incDetection()
{
  countDetection++;
  time_of_last_detection = Timestamp::now();
}

bool TopologyInfoNode::equals(EtherAddress *a)
{
  return (*a == node);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(TopologyInfoNode)
