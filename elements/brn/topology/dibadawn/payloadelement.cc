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
#include "payloadelement.hh"
#include "searchid.hh"
#include "neighbor.hh"
#include "dibadawn_packet.hh"

CLICK_DECLS

DibadawnPayloadElement::DibadawnPayloadElement(DibadawnSearchId &_id, EtherAddress &nodeA, EtherAddress &nodeB, bool _isBridge, uint8_t _hops)
:   isBridge(_isBridge),
    hops(_hops),
    cycle(_id, nodeA, nodeB)
{
  mayInconsistentlyData = new uint8_t[length];
}

DibadawnPayloadElement::DibadawnPayloadElement(const DibadawnPayloadElement &dple) : isBridge(dple.isBridge), hops(dple.hops), cycle(dple.cycle)
{
  mayInconsistentlyData = new uint8_t[length];
  memcpy(mayInconsistentlyData,dple.mayInconsistentlyData,length*sizeof(uint8_t));
}

DibadawnPayloadElement::DibadawnPayloadElement(const DibadawnCycle& c)
: isBridge(false),
  hops(1),
  cycle(c)
{
  mayInconsistentlyData = new uint8_t[length];
}

DibadawnPayloadElement::DibadawnPayloadElement(const uint8_t *p) :
  isBridge(false),
  hops(1),
  cycle()
{
  if ( p != NULL ) {
    isBridge = *p == 1;
    hops = (uint8_t)*(p + 1);
    cycle.setData(p + 2);
  }
  mayInconsistentlyData = new uint8_t[length];
}

void
DibadawnPayloadElement::setCycle(DibadawnCycle& c)
{
  cycle = c;
  isBridge = false;
  hops = 1;
}

uint8_t* DibadawnPayloadElement::getData()
{
  mayInconsistentlyData[0] = isBridge?1:0;
  memcpy(mayInconsistentlyData + 1, &hops, length - 1);
  memcpy(mayInconsistentlyData + 1 + sizeof(hops), cycle.getData(), length - (1 + sizeof(hops)));
  return (mayInconsistentlyData);
}

bool DibadawnPayloadElement::operator==(DibadawnPayloadElement &b)
{
  return (
      isBridge == b.isBridge &&
      cycle == b.cycle);
}

StringAccum& operator <<(StringAccum &output, const DibadawnPayloadElement &payload)
{
  output << "<PayloadElem ";
  output << "isBridge='" << payload.isBridge << "' ";
  output << "hops='" << int(payload.hops) << "' ";
  output << "cycleId='" << payload.cycle << "' ";
  output << "/>";

  return(output);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnPacketPayloadElement)
