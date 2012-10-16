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

#include "nhopneighbouring_compressed_protocol.hh"

CLICK_DECLS

WritablePacket *
NHopNeighbouringCompressedProtocol::new_ping(const EtherAddress *src, uint8_t hop_limit, NHopNeighbouringInfo *nhopi)
{
  Vector<EtherAddress> nhop_neighbours;

  int cnt_nhop_neighbours = nhopi->count_neighbours_max_hops(hop_limit);
  int nhopi_size = (hop_limit * sizeof(uint16_t)) + (cnt_nhop_neighbours * 6 * sizeof(uint8_t));
  WritablePacket *nhop_ping_packet = WritablePacket::make(128, NULL,
                                                     nhopi_size + sizeof(struct compressed_nhopn_header), 32);

  struct compressed_nhopn_header *nhopn_h = (struct compressed_nhopn_header *)nhop_ping_packet->data();

  nhopn_h->no_neighbours = htons(cnt_nhop_neighbours);
  nhopn_h->hop_limit = hop_limit;

  //nhop_counter follows the header
  uint16_t *nhop_counter = (uint16_t*)&(((struct compressed_nhopn_header *)nhop_ping_packet->data())[1]);
  //addr follows nhop counter
  uint8_t *addr_p = (uint8_t*)&(nhop_counter[hop_limit]);
  int addr_index = 0;


  nhop_neighbours.clear();

  for ( int h = 1; h <= hop_limit; h++ ) {
    nhopi->get_neighbours(&nhop_neighbours, h);
    nhop_counter[h-1] = (uint16_t)htons(nhop_neighbours.size());
    if ( nhop_neighbours.size() > 0 ) {
      for ( int n = 0; n < nhop_neighbours.size(); n++ ) {
        memcpy(&(addr_p[addr_index]), nhop_neighbours[n].data(), 6);
        addr_index += 6;
      }
    }
    nhop_neighbours.clear();
  }

  WritablePacket *brn_p = BRNProtocol::add_brn_header(nhop_ping_packet, BRN_PORT_COMP_NHOPNEIGHBOURING,
                                                                        BRN_PORT_COMP_NHOPNEIGHBOURING,
                                                                        2, 0);

  BRNPacketAnno::set_ether_anno(brn_p, src->data(), brn_ethernet_broadcast, ETHERTYPE_BRN);

  return brn_p;
}

void
NHopNeighbouringCompressedProtocol::unpack_ping(Packet *p, EtherAddress *src, uint16_t *no_neighbours, uint8_t *hop_limit)
{
  struct compressed_nhopn_header *nhopn_h = (struct compressed_nhopn_header *)p->data();
  click_ether *ether = (click_ether *)p->ether_header();

  *no_neighbours = ntohs(nhopn_h->no_neighbours);
  *hop_limit = nhopn_h->hop_limit;
  *src = EtherAddress(ether->ether_shost);
}

void
NHopNeighbouringCompressedProtocol::unpack_nhop(Packet *p, EtherAddress *src, uint8_t *hop_limit, NHopNeighbouringInfo *nhopi)
{
  struct compressed_nhopn_header *nhopn_h = (struct compressed_nhopn_header *)p->data();
  Vector<EtherAddress> neighbours;

  *hop_limit = nhopn_h->hop_limit;

  //nhop_counter follows the header
  uint16_t *nhop_counter = (uint16_t*)&(((struct compressed_nhopn_header *)p->data())[1]);
  //addr follows nhop counter
  uint8_t *addr_p = (uint8_t*)&(nhop_counter[*hop_limit]);
  int addr_index = 0;

  for ( int h = 0; h < ((*hop_limit)-1); h++ ) {
    uint16_t nhop_n = ntohs(nhop_counter[h]);
    for ( int n = 0; n < nhop_n; n++ ) {
      EtherAddress ea = EtherAddress(&(addr_p[addr_index]));
      addr_index += 6;
      nhopi->update_neighbour(&ea, h+2, 0, 0);
      neighbours.push_back(ea);
    }
  }

  nhopi->update_foreign_neighbours(src, &neighbours, *hop_limit);

}

CLICK_ENDDECLS
ELEMENT_PROVIDES(NHopNeighbouringCompressedProtocol)
