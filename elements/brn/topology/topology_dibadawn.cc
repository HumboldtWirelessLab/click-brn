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
#include "topology_dibadawn.hh"
#include "topology_dibadawn_packet.hh"

#define LOG BrnLogger(__FILE__, __LINE__, NULL).info

CLICK_DECLS;


DibadawnSearch::DibadawnSearch(BRNElement *brn_click_element, BRN2NodeIdentity *this_node_id)
{
  this->brn_click_element = brn_click_element;
  this->ownNodeId = this_node_id;
  this->search_id = DibadawnSearchId(Timestamp::now(), this_node_id->getMasterAddress());
}

String DibadawnSearch::AsString()
{
  return(this->search_id.AsString());
}

void DibadawnSearch::start_search()
{
  LOG("<--! start_search 1 -->");
  const EtherAddress* sender = ownNodeId->getMasterAddress();
  LOG("<--! start_search 2 -->");
  bool is_forward = true;
  DibadawnPacket packet(&search_id, sender, is_forward);
  LOG("<--! start_search 3 -->");
  WritablePacket *brn_packet = packet.getBrnPacket();
  LOG("<--! start_search 4 -->  0x%X", brn_packet);
  brn_click_element->output(0).push(brn_packet->clone());
  LOG("<--! start_search 5 -->");
}

void DibadawnSearch::receive(DibadawnPacket &packet)
{
  LOG("<--! Not Implemented yet -->");
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns)
ELEMENT_PROVIDES(DibadawnSearch)
