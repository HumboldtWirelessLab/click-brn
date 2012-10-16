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

#include "nhopneighbouring_protocol.hh"

CLICK_DECLS

WritablePacket *
NHopNeighbouringProtocol::new_ping(const EtherAddress *src, uint16_t no_neighbours, uint8_t hop_limit)
{
  WritablePacket *nhop_ping_packet = WritablePacket::make(128, NULL, sizeof(struct nhopn_header), 32);

  struct nhopn_header *nhopn_h = (struct nhopn_header *)nhop_ping_packet->data();

  nhopn_h->no_neighbours = htons(no_neighbours);
  nhopn_h->hop_limit = hop_limit;

  WritablePacket *brn_p = BRNProtocol::add_brn_header(nhop_ping_packet, BRN_PORT_NHOPNEIGHBOURING,
                                                                        BRN_PORT_NHOPNEIGHBOURING, hop_limit, 0);

  BRNPacketAnno::set_ether_anno(brn_p, src->data(), brn_ethernet_broadcast, ETHERTYPE_BRN);

  return brn_p;
}

void
NHopNeighbouringProtocol::unpack_ping(Packet *p, EtherAddress *src, uint16_t *no_neighbours, uint8_t *hop_limit, uint8_t *hops)
{
  struct nhopn_header *nhopn_h = (struct nhopn_header *)p->data();
  click_ether *ether = (click_ether *)p->ether_header();

  *no_neighbours = ntohs(nhopn_h->no_neighbours);
  *hop_limit = nhopn_h->hop_limit;
  *src = EtherAddress(ether->ether_shost);
  *hops = *hop_limit - BRNPacketAnno::ttl_anno(p);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(NHopNeighbouringProtocol)
