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

CLICK_DECLS;

#pragma push()
#pragma pack(1)
struct dibadawn_packet { 
  // TODO Optimize by size  
  uint8_t type;
  uint32_t id;
  uint8_t src[6];
  uint8_t ttl;
  uint8_t entries;
};
#pragma pop();

DibadawnSearch::DibadawnSearch(BRNElement *brn_click_element, BRN2NodeIdentity *this_node_id)
{
  this->brn_click_element = brn_click_element;
  this->node_id = node_id;
  this->search_id = DibadawnSearchId(Timestamp::now(), this_node_id->getMasterAddress());
  BrnLogger(__FILE__, __LINE__, NULL).info("<!-- huhu -->");
  
  
  const EtherAddress* sender = this_node_id->getMasterAddress();
  
  const uint8_t *sender_bytes = sender->data();
  
  
  dibadawn_packet content;
  content.type = 0;
  content.id = htonl(88);
  memcpy(content.src, sender_bytes, 6);
  content.entries = 0;
  content.ttl = 87; // Does this makes sense?

  uint32_t sizeof_head = 128;
  uint32_t sizeof_data = sizeof(content);
  uint32_t sizeof_tail = 32;
  WritablePacket *packet = WritablePacket::make( 
        sizeof_head,
        &content, sizeof_data, 
        sizeof_tail);
  
  WritablePacket *brn_packet = BRNProtocol::add_brn_header(
          packet, 
          BRN_PORT_TOPOLOGY_DETECTION,
          BRN_PORT_TOPOLOGY_DETECTION, 
          128, 
          0);

  BRNPacketAnno::set_ether_anno(
        brn_packet, 
        sender_bytes, 
        brn_ethernet_broadcast, 
        ETHERTYPE_BRN);

  BrnLogger(__FILE__, __LINE__, NULL).info("<!-- huhu2 -->");
  //brn_click_element->push(0, brn_packet);
  brn_click_element->output(0).push(brn_packet);
}

String DibadawnSearch::AsString()
{
  return(this->search_id.AsString());
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns)
ELEMENT_PROVIDES(DibadawnSearch)
