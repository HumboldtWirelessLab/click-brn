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
#ifndef NHOPNEIGHBOURING_PROTOCOL_EEWS_HH
#define NHOPNEIGHBOURING_PROTOCOL_EEWS_HH

#include "elements/brn2/services/sensor/gps/gps.hh"

#include "elements/brn2/services/sensor/alarmingprotocol/alarmingstate.hh"

#include <click/element.hh>

CLICK_DECLS

CLICK_SIZE_PACKED_STRUCTURE(
struct nhopn_header {,
  uint32_t no_neighbours;
  uint8_t hop_limit;
  uint32_t latitude;
  uint32_t longitude;
  uint32_t altitude;
  uint8_t state;
});

class NHopNeighbouringProtocolEews {

 public:

  static WritablePacket *new_ping(const EtherAddress *src, uint32_t no_neighbours, uint8_t hop_limit, GPSPosition *gpspos, uint8_t state);
  static void unpack_ping(Packet *p, EtherAddress *src, uint32_t *no_neighbours, uint8_t *hop_limit, uint8_t *hops, GPSPosition *gpspos, uint8_t *state);
};

CLICK_ENDDECLS
#endif
