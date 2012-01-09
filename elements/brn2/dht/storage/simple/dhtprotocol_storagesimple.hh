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

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#define DHT_STORAGE_SIMPLE_MESSAGE         1
#define DHT_STORAGE_SIMPLE_MOVEDDATA       2
#define DHT_STORAGE_SIMPLE_ACKDATA         3

CLICK_DECLS

//TODO: This should maybe push to DHTprotocol (dht_packet_header)
struct dht_simple_storage_node_info {
  uint8_t  src_id_size;
  uint8_t  reserved;
  uint8_t  src_id[MD5_DIGEST_LENGTH];
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct dht_simple_storage_data {
  uint32_t move_id;
  uint8_t  count_rows;
  uint8_t  reserved;
} CLICK_SIZE_PACKED_ATTRIBUTE;

class DHTProtocolStorageSimple {

  public:

    /**
     * Funtions for DHTOperation packets (creation, forward,...)
    */
    static WritablePacket *new_dht_operation_packet(DHTOperation *op, DHTnode *src, EtherAddress *dst, bool _add_node_id);
    static struct dht_simple_storage_node_info *unpack_dht_operation_packet(Packet *packet, DHTOperation *op, EtherAddress *src, bool _add_node_id);
    static void inc_hops_of_dht_operation_packet(Packet *packet, bool _add_node_id);

    /**
     * Functions for handling packets during data transfer coused by routing-table update
    */
    static WritablePacket *new_data_packet(EtherAddress *src, int32_t moveID, uint8_t countRows, uint16_t data_size);
    static uint8_t *get_data_packet_payload(Packet *p);
    static struct dht_simple_storage_data *get_data_packet_header(Packet *p);

    static WritablePacket *new_ack_data_packet(int32_t moveID);
    static uint32_t get_ack_movid(Packet *p);

};

CLICK_ENDDECLS
#endif
