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
#include <click/timestamp.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "searchid.hh"

CLICK_DECLS


DibadawnSearchId::DibadawnSearchId()
{
  memset(data, 0, sizeof (data));
}

DibadawnSearchId::DibadawnSearchId(Timestamp t, const EtherAddress &creator)
{
  set(t, creator);
}

uint8_t* DibadawnSearchId::PointerTo10BytesOfData()
{
  return (data);
}

void DibadawnSearchId::setByPointerTo10BytesOfData(const uint8_t *value)
{
  memcpy(data, value, sizeof (data));
}

void DibadawnSearchId::set(DibadawnSearchId &id)
{
  memcpy(data, id.data, sizeof(data));
}

void DibadawnSearchId::set(Timestamp t, const EtherAddress& creator)
{
  const uint8_t *pmac = reinterpret_cast<const uint8_t *> (creator.data());
  memcpy(this->data, pmac, 6);

  uint32_t time = t.sec();
  uint32_t netTime = htonl(time);
  memcpy(this->data + 6, &netTime, sizeof (netTime));
}

DibadawnSearchId & DibadawnSearchId::operator =(const DibadawnSearchId &id)
{
  if (this != &id)
    memcpy(this->data, id.data, sizeof (this->data));

  return *this;
}

bool DibadawnSearchId::isEqualTo(DibadawnSearchId &id)
{
  return (memcmp(data, id.data, sizeof (data)) == 0);
}

String DibadawnSearchId::asString()
{
  StringAccum as;
  as << *this;
  return(as.take_string());
}


StringAccum& operator <<(StringAccum &output, const DibadawnSearchId &id)
{
  uint8_t *mac = (uint8_t*) & id.data[0];
  uint32_t *time = (uint32_t*) & id.data[6];
  output.snprintf(27, "%02X-%02X-%02X-%02X-%02X-%02X-%08X",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
      *time);

  return(output);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnSearchId)
