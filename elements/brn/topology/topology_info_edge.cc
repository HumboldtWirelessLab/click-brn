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

#include "topology_info_edge.hh"

CLICK_DECLS


TopologyInfoEdge::TopologyInfoEdge(EtherAddress *a, EtherAddress *b, float p=0.0)
:   time_of_last_detection(Timestamp::now()),
    probability(p)
{
  node_a = EtherAddress(a->data());
  node_b = EtherAddress(b->data());
  countDetection = 1;
}

void TopologyInfoEdge::incDetection()
{
  countDetection++;
  time_of_last_detection = Timestamp::now();
}

bool TopologyInfoEdge::equals(EtherAddress *a, EtherAddress *b)
{
  return (((memcmp(node_a.data(), a->data(), 6) == 0) && (memcmp(node_b.data(), b->data(), 6) == 0)) ||
      ((memcmp(node_a.data(), b->data(), 6) == 0) && (memcmp(node_b.data(), a->data(), 6) == 0)));
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(TopologyInfoEdge)
