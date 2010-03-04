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

#ifndef GATEWAY_H
#define GATEWAY_H

CLICK_SIZE_PACKED_STRUCTURE(
struct brn_gateway_dht_entry {,
  uint8_t  etheraddress[6];
  uint32_t ipv4;
  uint32_t bandwidth;
  uint8_t  metric;
  uint8_t  flags;
#define GATEWAY_FLAG_NATED 1
});

CLICK_SIZE_PACKED_STRUCTURE(
struct brn_gateway {,
  //uint8_t failed;  /* >= 1, if packet failed */
    uint8_t metric;  /* metric for gateway, which sent this feedback */
});

//
CLICK_SIZE_PACKED_STRUCTURE(
struct flow_gw {,
  uint32_t    src;
  uint16_t    sport;
  uint32_t    dst;
  uint16_t    dport;
  uint8_t     gw[6];
});


CLICK_SIZE_PACKED_STRUCTURE(
struct brn_gateway_handover {,
  uint16_t    length;
  flow_gw*    flows; // length many flows
});

#endif
