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
#include <click/element.hh>
#include <clicknet/ether.h>
//#include "elements/brn/common.hh"

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "dhtprotocol.hh"

CLICK_DECLS

WritablePacket *
DHTProtocol::new_dht_packet(uint8_t major_type, uint8_t minor_type,uint16_t payload_len)
{
  WritablePacket *new_packet = NULL;
  struct dht_packet_header *dht_header = NULL;

  new_packet = WritablePacket::make( 128, NULL, sizeof(struct dht_packet_header) + payload_len, 32);  //TODO:check size of headroom (what is needed)
  dht_header = (struct dht_packet_header *)new_packet->data();

  dht_header->major_type = major_type;
  dht_header->minor_type = minor_type;
  dht_header->payload_len = htons(payload_len);

  return(new_packet);	
}

uint8_t
DHTProtocol::get_routing(Packet *p)
{
  struct dht_packet_header *dht_header = (struct dht_packet_header *)p->data();
  return ( dht_header->major_type );
}

uint8_t
DHTProtocol::get_type(Packet *p)
{
  struct dht_packet_header *dht_header = (struct dht_packet_header *)p->data();
  return ( dht_header->minor_type );
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

EtherAddress *
DHTProtocol::get_src(Packet *p)
{
  struct dht_packet_header *dht_header = NULL;

  if ( p != NULL  && p->length() >= sizeof(struct dht_packet_header) )
  {
    dht_header = (struct dht_packet_header*)p->data();
    return (new EtherAddress(dht_header->src));
  }
  else
    return NULL;
}

EtherAddress *
DHTProtocol::get_dst(Packet *p)
{
  struct dht_packet_header *dht_header = NULL;

  if ( p != NULL  && p->length() >= sizeof(struct dht_packet_header) )
  {
    dht_header = (struct dht_packet_header*)p->data();
    return (new EtherAddress(dht_header->dst));
  }
  else
    return NULL;
}

uint8_t *
DHTProtocol::get_src_data(Packet *p)
{
  if ( p != NULL  && p->length() >= sizeof(struct dht_packet_header) )
    return ((struct dht_packet_header*)p->data())->src;
  else
    return NULL;
}

uint8_t *
DHTProtocol::get_dst_data(Packet *p)
{
  if ( p != NULL  && p->length() >= sizeof(struct dht_packet_header) )
    return ((struct dht_packet_header*)p->data())->dst;
  else
    return NULL;
}

int
DHTProtocol::set_src(Packet *p, uint8_t *ea)
{
  struct dht_packet_header *dht_header = NULL;

  if ( p != NULL  && p->length() >= sizeof(struct dht_packet_header) )
  {
    dht_header = (struct dht_packet_header*)p->data();
    memcpy(dht_header->src,ea,6);
    return 0;
  }
  else
    return -1;
}

int
DHTProtocol::set_dst(Packet *p, uint8_t *ea)
{
  struct dht_packet_header *dht_header = NULL;

  if ( p != NULL  && p->length() >= sizeof(struct dht_packet_header) )
  {
    dht_header = (struct dht_packet_header*)p->data();
    memcpy(dht_header->dst,ea,6);
    return 0;
  }
  else
    return -1;
}


WritablePacket *
DHTProtocol::push_brn_ether_header(WritablePacket *p,EtherAddress *src, EtherAddress *dst, uint8_t major_type)
{
  WritablePacket *big_p = NULL;
  struct click_brn brn_header;
  click_ether *ether_header = NULL;
  int payload_len = p->length();

  big_p = p->push(sizeof(struct click_brn) + sizeof(click_ether));

  if ( big_p == NULL ) {
    click_chatter("Push failed. No memory left ??");
  }
  else
  {
    ether_header = (click_ether *)big_p->data();
    memcpy( ether_header->ether_dhost,dst->data(),6);
    memcpy( ether_header->ether_shost,src->data(),6);
    ether_header->ether_type = htons(ETHERTYPE_BRN);
    big_p->set_ether_header(ether_header);

    brn_header.dst_port = major_type;
    brn_header.src_port = major_type;
    brn_header.body_length = payload_len;
    brn_header.ttl = 100;
    brn_header.tos = 0;

    memcpy((void*)&(big_p->data()[14]),(void*)&brn_header, sizeof(brn_header));
  }

  return big_p;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DHTProtocol)
