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

#ifndef CLICK_COOPERATIVECHANNELSTATS_PROTOCOL_HH
#define CLICK_COOPERATIVECHANNELSTATS_PROTOCOL_HH

CLICK_DECLS

#define INCLUDES_NEIGHBOURS 1

/**
 * TODO: - queue size
 *       - (diff between ctl, mngt, data frame)
 *
 */
CLICK_SIZE_PACKED_STRUCTURE(,
struct local_airtime_stats {
  uint32_t stats_id;

  uint32_t duration;

  uint8_t hw_rx;
  uint8_t hw_tx;
  uint8_t hw_busy;

  uint8_t mac_rx;
  uint8_t mac_tx;
  uint8_t mac_busy;

  uint32_t tx_pkt_count;
  uint32_t tx_byte_count;

  uint32_t rx_pkt_count;
  uint32_t rx_byte_count;
});

CLICK_SIZE_PACKED_STRUCTURE(,
struct neighbour_airtime_stats {
  uint8_t _etheraddr[6];

  uint32_t _rx_pkt_count;
  uint32_t _rx_byte_count;

  uint8_t _min_rssi;
  uint8_t _max_rssi;

  uint8_t _avg_rssi;
  uint8_t _std_rssi;

  //TODO: relative values (percent) is better?
  uint32_t _duration; //tx Duration??
});

struct cooperative_channel_stats_header {
    uint16_t endianess;      // endianess-test
    uint8_t flags;          // flags
    uint8_t no_neighbours;  // number of neighbours
    struct local_airtime_stats local_stats;
};

struct cooperative_channel_stats_body {
    struct neighbour_airtime_stats neighbour_stats;
};

CLICK_ENDDECLS
#endif
