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
#ifndef DHT_PROTOCOL_FALCON_HH
#define DHT_PROTOCOL_FALCON_HH

#include <click/element.hh>

CLICK_DECLS

#define FALCON_RT_POSITION_SUCCESSOR   0
#define FALCON_RT_POSITION_PREDECESSOR 255

#define FALCON_OPERATION_REQUEST_SUCCESSOR   1
#define FALCON_OPERATION_REQUEST_PREDECESSOR 2
#define FALCON_OPERATION_REQUEST_POSITION    3
#define FALCON_OPERATION_UPDATE_POSITION     4

#define FALCON_STATUS_OK    0
#define FALCON_STATUS_HINT  1

struct dht_falcon_node_entry {
  uint8_t  etheraddr[6];
  uint8_t  age_sec;
  uint8_t  status;
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct falcon_routing_packet {
  uint8_t operation;             //TODO: this is already in generic DHT-Header. Isn't it.
  uint8_t status;

  uint8_t table_position;
  uint8_t reserved;

  uint8_t etheraddr[6];

  uint8_t src_ea[6];

} CLICK_SIZE_PACKED_ATTRIBUTE;


/**
 * Packet: me, pred, succ, fingertable
*/

class DHTProtocolFalcon {

  public:

    static int pack_lp(uint8_t *buffer, int buffer_len, DHTnode *me, DHTnodelist *nodes);
    static int unpack_lp(uint8_t *buffer, int buffer_len, DHTnode *first, DHTnodelist *nodes);

    static WritablePacket *new_route_request_packet(DHTnode *src, DHTnode *dst, uint8_t operation, int request_position);
    static WritablePacket *new_route_reply_packet(DHTnode *src, DHTnode *dst, uint8_t operation, DHTnode *node, int request_position);
    static WritablePacket *fwd_route_request_packet(DHTnode *src, DHTnode *new_dst, DHTnode *fwd, Packet *p);
    //static WritablePacket *route_reply_packet(Packet *p, DHTnode *me, DHTnode *pred, DHTnode *succ, DHTnodelist *finger);

    static int get_operation(Packet *p);
    static DHTnode *get_src(Packet *p);

    static void get_info(Packet *p, DHTnode *src, DHTnode *node, uint8_t *operation, uint8_t *status, uint8_t *pos);

};

CLICK_ENDDECLS
#endif
