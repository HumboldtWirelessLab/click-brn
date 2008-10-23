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
#include "elements/brn/common.hh"

#include "dhtprotocol.hh"

CLICK_DECLS

WritablePacket *
DHTProtocol::new_dht_packet(uint8_t routing, uint8_t type,uint16_t payload_len)
{
  WritablePacket *new_packet = NULL;
  struct dht_packet_header *dht_header = NULL;

  new_packet = WritablePacket::make( sizeof(struct dht_packet_header) + payload_len);
  dht_header = (struct dht_packet_header *)new_packet->data();

  dht_header->routing = routing;
  dht_header->type = type;
  dht_header->payload_len = htons(payload_len);

  return(new_packet);	
}

uint8_t
DHTProtocol::get_routing(Packet *p)
{
  struct dht_packet_header *dht_header = (struct dht_packet_header *)p->data();
  return ( dht_header->routing );
}

uint8_t
DHTProtocol::get_type(Packet *p)
{
  struct dht_packet_header *dht_header = (struct dht_packet_header *)p->data();
  return ( dht_header->type );
}

uint16_t
DHTProtocol::get_payload_len(Packet *p)
{
  struct dht_packet_header *dht_header = (struct dht_packet_header *)p->data();
  return ( ntohs(dht_header->payload_len) );
}

uint8_t *
DHTProtocol::get_payload(Packet *p)
{
  if ( p != NULL  && p->length() >= sizeof(struct dht_packet_header) )
    return ((uint8_t*)&(p->data()[sizeof(struct dht_packet_header)]));
  else
    return NULL;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DHTProtocol)
