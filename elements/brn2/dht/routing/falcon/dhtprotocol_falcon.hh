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

#define HAS_SUCCESSOR   128
#define HAS_PREDECESSOR  64
#define FINGERTABLESIZE  63

struct dht_falcon_header {
  uint8_t  status;
};

struct dht_falcon_node_entry {
  uint8_t  etheraddr[6];
  uint8_t  age_sec;
  uint8_t  status;
};


/**
 * Packet: me, pred, succ, fingertable
*/

class DHTProtocolFalcon {

  public:

    static int pack_lp(uint8_t *buffer, int buffer_len, DHTnode *me, DHTnodelist *nodes);
    static int unpack_lp(uint8_t *buffer, int buffer_len, DHTnode *first, DHTnodelist *nodes);

    static WritablePacket *new_hello_packet(EtherAddress *etheraddr);
    static WritablePacket *new_hello_request_packet(EtherAddress *etheraddr);

    static WritablePacket *new_route_request_packet(DHTnode *me);
    static WritablePacket *new_route_reply_packet(DHTnode *me, DHTnode *pred, DHTnode *succ, DHTnodelist *finger);

    static DHTnode     *get_src(Packet *p);
    static DHTnode     *get_successor(Packet *p);
    static DHTnode     *get_predecessor(Packet *p);
    static DHTnodelist *get_fingertable(Packet *p);
    static DHTnodelist *get_all_nodes(Packet *p);
};

CLICK_ENDDECLS
#endif
