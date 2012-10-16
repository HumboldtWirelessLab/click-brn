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
#ifndef DHT_PROTOCOL_KLIBS_HH
#define DHT_PROTOCOL_KLIBS_HH

#include <click/element.hh>

CLICK_DECLS

#define KLIBS_HELLO           1
#define KLIBS_REQUEST         2
#define KLIBS_REQUEST_OWN     3
#define KLIBS_REQUEST_FOREIGN 4

#define KLIBS_VERSION 0x01

struct klibs_protocolheader {
  uint8_t  version;
  uint8_t  entries;            //number of entries in the packet
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct klibs_rt_entry {
  uint8_t  etheraddr[6];
  uint32_t age;
} CLICK_SIZE_PACKED_ATTRIBUTE;

class DHTProtocolKlibs {

  public:

    static WritablePacket *new_packet(EtherAddress *src, EtherAddress *dst, uint8_t ptype, DHTnodelist *dhtlist);
    static int get_dhtnodes(Packet *p,uint8_t *ptype, DHTnodelist *dhtlist);

};

CLICK_ENDDECLS
#endif
