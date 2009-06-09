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
#ifndef DHT_PROTOCOL_HH
#define DHT_PROTOCOL_HH

#include <click/element.hh>

CLICK_DECLS

#define BRN_PORT_DHTROUTING 10
#define BRN_PORT_DHTSTORAGE 11

/************* M A J O R *******************/
#define ROUTING_OMNI        1
#define ROUTING_FALCON      2
#define ROUTING_KLIBS       3
#define ROUTING_DART        4

#define STORAGE_SIMPLE      128

/***** M I N O R  - R O U T I N G **********/

#define HELLO               1
#define HELLO_REQUEST       2
#define ROUTETABLE_REQUEST  3
#define ROUTETABLE_REPLY    4
#define ROUTE_REQUEST       5
#define ROUTE_REPLAY        6

/***** M I N O R  - S T O R A G E **********/

#define DHT_MESSAGE         1
#define DHT_MOVEDDATA       2


struct dht_packet_header {
  uint8_t  major_type;
  uint8_t  minor_type;
  uint8_t  src[6];
  uint8_t  dst[6];
  uint16_t payload_len;
};

#define DHT_PACKET_HEADER_SIZE sizeof(dht_packet_header)

class DHTProtocol {
  public:
    static WritablePacket *new_dht_packet(uint8_t major_type, uint8_t minor_type, uint16_t payload_len);
    static struct dht_packet_header *get_header(Packet *p);

    static WritablePacket *push_brn_ether_header(WritablePacket *p, EtherAddress *src, EtherAddress *dst, uint8_t major_type);


    static uint8_t get_routing(Packet *p);
    static uint8_t get_type(Packet *p);
    static uint16_t get_payload_len(Packet *p);
    static uint8_t *get_payload(Packet *p);

    static EtherAddress *get_src(Packet *p);
    static EtherAddress *get_dst(Packet *p);

    static int set_src(Packet *p, uint8_t *ea);
    static int set_dst(Packet *p, uint8_t *ea);

};

CLICK_ENDDECLS
#endif
