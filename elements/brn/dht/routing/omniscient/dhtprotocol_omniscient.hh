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
#ifndef DHT_PROTOCOL_OMNI_HH
#define DHT_PROTOCOL_OMNI_HH

#include <click/element.hh>

CLICK_DECLS

struct dht_omni_node_entry {
  uint8_t  etheraddr[6];
  uint8_t  age_sec;
  uint8_t  status;
};

class DHTProtocolOmni {

  public:

    static WritablePacket *new_hello_packet(EtherAddress *etheraddr);
    static WritablePacket *new_hello_request_packet(EtherAddress *etheraddr);
    static WritablePacket *new_route_request_packet(EtherAddress *me, DHTnodelist *list);
    static WritablePacket *new_route_reply_packet(EtherAddress *me, DHTnodelist *list);
    static int get_dhtnodes(Packet *p,DHTnodelist *dhtlist);
    static WritablePacket *push_brn_ether_header(WritablePacket *p,EtherAddress *src, EtherAddress *dst);

};

CLICK_ENDDECLS
#endif