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

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "topology_dibadawn.hh"

CLICK_DECLS


DibadawnSearchId::DibadawnSearchId(Timestamp t, EtherAddress creator) 
{
  ;
}
    
String DibadawnSearchId::AsString() 
{
  String str = String::make_uninitialized(17);
  char *x = str.mutable_c_str();
  if (x == NULL) 
    throw;

  uint8_t *mac = (uint8_t*)&data[0];
  uint32_t *time = (uint32_t*)&data[6];
  sprintf(x, "%02X-%02X-%02X-%02X-%02X-%02X-%08X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
    *time);
  return(str);
}


uint8_t* DibadawnSearchId::PointerTo10BytesOfData() 
{
  return(data);
}

DibadawnSearch::DibadawnSearch(BRNElement *brn_click_element, EtherAddress creator) 
{
  this->brn_click_element = brn_click_element;
  this->search_id = DibadawnSearchId(Timestamp::now(), creator);
}

String DibadawnSearch::AsString()
{
  return(this->search_id.AsString());
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(TopologyDibadawn)
