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
#include "edgemarking.hh"

CLICK_DECLS

DibadawnEdgeMarking::DibadawnEdgeMarking(DibadawnSearchId& id, bool isBridge, EtherAddress& nodeA, EtherAddress& nodeB, bool isTrusted)
{
  this->id = id;
  this->isBridge = isBridge;
  this->nodeA = nodeA;
  this->nodeB = nodeB;
  this->isTrusted = isTrusted;
      
  this->time = Timestamp::now();
  this->competence = 1.0;
}

DibadawnEdgeMarking::DibadawnEdgeMarking(DibadawnSearchId& id, bool isBridge, EtherAddress& nodeA, EtherAddress& nodeB, double competence)
{
  this->id = id;
  this->isBridge = isBridge;
  this->nodeA = nodeA;
  this->nodeB = nodeB;
  this->competence = competence;
  
  this->isTrusted = false;
  this->time = Timestamp::now();
}


DibadawnEdgeMarking::DibadawnEdgeMarking()
{
  isTrusted = false;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnEdgeMarking)