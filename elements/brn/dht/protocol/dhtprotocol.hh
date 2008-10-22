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
#ifndef DHT_COMM_HH
#define DHT_COMM_HH


#include <click/element.hh>

CLICK_DECLS

#define DHT  1
#define DHCP 2
#define ARP  3
#define GATEWAY 4
#define DHTTESTER 5
#define VLANS 6

/* Operations */
/* field: code */

#define READ       1
#define INSERT     2
#define WRITE      4
#define REMOVE     8
#define LOCK      16
#define UNLOCK    32
#define DHT_DATA  64
#define ROUTING  128

/*old stuff*/
#define KEY_FOUND     0
#define KEY_NOT_FOUND 1

/*field: flags */
#define STATUS_OK     0
#define DHT_DATA_OK 128

/* payload Types */
#define TYPE_UNKNOWN  0
#define TYPE_SUB_LIST 1
#define TYPE_MAC      2
#define TYPE_IP       3
#define TYPE_REL_TIME 4
#define TYPE_STRING   5
#define TYPE_INTEGER  6

struct dht_packet_header {
  uint8_t  sender;
  uint8_t  receiver;
  uint8_t  sender_hwa[6];
  uint8_t  prim_sender;
  uint8_t  flags;
  uint8_t  code;
  uint8_t  id;
  uint16_t payload_len;
};


#define ROUTE_REQUEST 1
#define ROUTE_REPLY   2
#define ROUTE_INFO    4

struct dht_routing_header {
  uint8_t  _type;
  uint8_t  _num_send_nodes;     //number of nodes in packet
  uint8_t  _type_of_nodes;
  uint32_t _num_dht_nodes;      //number of known DHT-Nodes
};

struct dht_data_header {
  uint8_t  _type;                //remove or insert (not used)
  uint8_t  _num_dht_entries;     //number of DHTentries in packet
};

struct dht_node_info {
  uint8_t  _hops;
  uint8_t  _ether_addr[6];
};

struct dht_packet_payload {
  uint8_t       number;
  uint8_t       type;
  uint8_t       len;
  uint8_t       data[10];
};

#define DHT_PACKET_HEADER_SIZE sizeof(dht_packet_header)

//#define DHT_PAYLOAD_OVERHEAD sizeof(dht_packet_payload) - sizeof(uint8_t*)
#define DHT_PAYLOAD_OVERHEAD 3


class DHTProtocol {

WritablePacket *new_dht_packet();

int set_header(Packet *dht_packet, uint8_t sender, uint8_t receiver, uint8_t flags, uint8_t code, uint8_t id );

WritablePacket * add_payload_list(Packet *dht_packet,struct dht_packet_payload *payload_list, int payload_list_len);
WritablePacket * add_payload(Packet *dht_packet, uint8_t number, uint8_t type, uint8_t len, uint8_t* data);

WritablePacket * add_sub_list(Packet *dht_packet, uint8_t number);

int count_values(Packet *dht_packet);
struct dht_packet_payload *get_payload_by_number(Packet *dht_packet, uint8_t num);

int dht_pack_payload(Packet *dht_packet);
int dht_pack_payload(uint8_t *new_dht_payload, int len);

int dht_unpack_payload(Packet *dht_packet);
int dht_unpack_payload(uint8_t *new_dht_payload, int len);
};

CLICK_ENDDECLS
#endif
