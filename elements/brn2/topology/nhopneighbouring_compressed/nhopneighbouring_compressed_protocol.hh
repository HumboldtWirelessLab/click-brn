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

/* Sender-/Receivernumbers */
/* field: sender,receiver */
#ifndef NHOPNEIGHBOURING_COMPRESSED_PROTOCOL_HH
#define NHOPNEIGHBOURING_COMPRESSED_PROTOCOL_HH

#include <click/element.hh>

#include "elements/brn2/topology/nhopneighbouring/nhopneighbouring_info.hh"

CLICK_DECLS

CLICK_SIZE_PACKED_STRUCTURE(
struct compressed_nhopn_header {,
  uint16_t no_neighbours;
  uint8_t hop_limit;
});

class NHopNeighbouringCompressedProtocol {

 public:

  static WritablePacket *new_ping(const EtherAddress *src, uint8_t hop_limit, NHopNeighbouringInfo *nhopi);

  static void unpack_ping(Packet *p, EtherAddress *src, uint16_t *no_neighbours, uint8_t *hop_limit);
  static void unpack_nhop(Packet *p, EtherAddress *src, uint8_t *hop_limit, NHopNeighbouringInfo *nhopi);

};

CLICK_ENDDECLS
#endif
