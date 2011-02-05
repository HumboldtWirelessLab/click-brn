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

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"

CLICK_DECLS

#define FALCON_RT_POSITION_SUCCESSOR   0
#define FALCON_RT_POSITION_PREDECESSOR 65535

/***** M I N O R  - R O U T I N G **********/
#define FALCON_MINOR_REQUEST_SUCCESSOR              1
#define FALCON_MINOR_UPDATE_SUCCESSOR               2
#define FALCON_MINOR_REPLY_SUCCESSOR                3
#define FALCON_MINOR_REQUEST_PREDECESSOR            4
#define FALCON_MINOR_REPLY_PREDECESSOR              5
#define FALCON_MINOR_REQUEST_POSITION               6
#define FALCON_MINOR_REPLY_POSITION                 7
#define FALCON_MINOR_LEAVE_NETWORK_NOTIFICATION     8
#define FALCON_MINOR_LEAVE_NETWORK_REPLY            9
#define FALCON_MINOR_PASSIVE_MONITORING_ACTIVATE   10
#define FALCON_MINOR_PASSIVE_MONITORING_DEACTIVATE 11

#define FALCON_MINOR_NWS_REQUEST               10

/**
 * structure is used for linkprobes
 */
struct dht_falcon_node_entry {
  uint8_t  etheraddr[6];
  uint8_t  age_sec;
  uint8_t  status;
  md5_byte_t node_id[MAX_NODEID_LENTGH];
} CLICK_SIZE_PACKED_ATTRIBUTE;

/**
 * structure is used for routerequests
 */
struct falcon_routing_packet {
  uint8_t node_status;
  uint8_t src_status;

  uint16_t table_position;
  uint8_t  etheraddr[6];                      //Etheraddress of the node on position "table_position" in the table
  md5_byte_t node_id[MAX_NODEID_LENTGH];      //Node-id of the node on position "table_position" in the table

  md5_byte_t src_node_id[MAX_NODEID_LENTGH];  //node-id of the src of the request/reply. TODO: move this to general DHT header ??
} CLICK_SIZE_PACKED_ATTRIBUTE;

/**
 * structure is used to determinate the networksize
 */
struct falcon_nws_packet {
  uint32_t networksize;

  md5_byte_t src_node_id[MAX_NODEID_LENTGH];  //node-id of the src of the request/reply. TODO: move this to general DHT header ??
} CLICK_SIZE_PACKED_ATTRIBUTE;

/**
 * Packet: me, pred, succ, fingertable
*/

class DHTProtocolFalcon {

  public:

    static int max_no_nodes_in_lp(int buffer_len);
    static int pack_lp(uint8_t *buffer, int buffer_len, DHTnode *me, DHTnodelist *nodes);
    static int unpack_lp(uint8_t *buffer, int buffer_len, DHTnode *first, DHTnodelist *nodes);

    static WritablePacket *new_route_request_packet(DHTnode *src, DHTnode *dst, uint8_t operation, int request_position);
    static WritablePacket *new_route_reply_packet(DHTnode *src, DHTnode *dst, uint8_t operation, DHTnode *node, int request_position);
    static WritablePacket *fwd_route_request_packet(DHTnode *src, DHTnode *new_dst, DHTnode *fwd, Packet *p);

    static WritablePacket *new_route_leave_packet(DHTnode *src, DHTnode *dst, uint8_t operation, DHTnode *node, int request_position);

    static void get_info(Packet *p, DHTnode *src, DHTnode *node, uint16_t *pos);

    static WritablePacket *new_nws_packet(DHTnode *src, DHTnode *dst, uint32_t size);
    static WritablePacket *fwd_nws_packet(DHTnode *src, DHTnode *next, uint32_t size, Packet *p);
    static void get_nws_info(Packet *p, DHTnode *src, uint32_t *size);

};

CLICK_ENDDECLS
#endif
