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
#include "topology_dibadawn_cycle.hh"

CLICK_DECLS


DibadawnCycle::DibadawnCycle()
{
  memset(contentAsBytes, '\0', sizeof(contentAsBytes));
}

DibadawnCycle::DibadawnCycle(DibadawnSearchId &id, EtherAddress &addr1, EtherAddress &addr2)
{
  memcpy(contentAsBytes, id.PointerTo10BytesOfData(), 10);

  if (addr1.hashcode() >= addr2.hashcode())
  {
    const uint8_t *pmac1 = reinterpret_cast<const uint8_t *> (addr1.data());
    memcpy(contentAsBytes + 10, pmac1, 6);

    const uint8_t *pmac2 = reinterpret_cast<const uint8_t *> (addr2.data());
    memcpy(contentAsBytes + 10 + 6, pmac2, 6);
  }
  else
  {
    const uint8_t *pmac2 = reinterpret_cast<const uint8_t *> (addr2.data());
    memcpy(contentAsBytes + 10, pmac2, 6);

    const uint8_t *pmac1 = reinterpret_cast<const uint8_t *> (addr1.data());
    memcpy(contentAsBytes + 10 + 6, pmac1, 6);
  }

}

String DibadawnCycle::AsString()
{
  String str = String::make_uninitialized(26 + 2 + 17 + 2 + 17 + 1);
  char *x = str.mutable_c_str();
  assert(x != NULL);

  uint8_t *mac0 = (uint8_t*) & contentAsBytes[0];
  uint32_t *time = (uint32_t*) & contentAsBytes[6];
  uint8_t *mac1 = (uint8_t*) & contentAsBytes[10];
  uint8_t *mac2 = (uint8_t*) & contentAsBytes[16];
  sprintf(x, "%02X-%02X-%02X-%02X-%02X-%02X-%08X--%02X-%02X-%02X-%02X-%02X-%02X--%02X-%02X-%02X-%02X-%02X-%02X",
      mac0[0], mac0[1], mac0[2], mac0[3], mac0[4], mac0[5],
      *time,
      mac1[0], mac1[1], mac1[2], mac1[3], mac1[4], mac1[5],
      mac2[0], mac2[1], mac2[2], mac2[3], mac2[4], mac2[5]);

  return (str);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnCycle)
