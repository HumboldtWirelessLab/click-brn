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
#ifndef DHT_PROTOCOL_STORAGESIMPLE_HH
#define DHT_PROTOCOL_STORAGESIMPLE_HH

#include <click/element.hh>

#define DHT_STORAGE_SIMPLE_MESSAGE         1
#define DHT_STORAGE_SIMPLE_MOVEDDATA       2

CLICK_DECLS

//TODO: This should maybe push to DHTprotocol (dht_packet_header)
struct dht_simple_storage_node_info {
  uint8_t  src_id_size;
  uint8_t  reserved;
  uint8_t  src_id[16];
};

struct dht_simple_storage_data {
  uint8_t  etheraddr[6];
};

class DHTProtocolStorageSimple {

  public:

    static WritablePacket *new_data_packet(int32_t moveID, int countRows, uint8_t *data, uint16_t data_size);

};

CLICK_ENDDECLS
#endif
