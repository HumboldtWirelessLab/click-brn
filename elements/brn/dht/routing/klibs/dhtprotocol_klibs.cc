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
#include <click/config.h>
#include <click/etheraddress.hh>

#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"
#include "elements/brn/dht/protocol/dhtprotocol.hh"
#include "dhtprotocol_klibs.hh"

CLICK_DECLS

WritablePacket *
DHTProtocolKlibs::new_packet(EtherAddress *src, EtherAddress */*dst*/, uint8_t ptype, DHTnodelist *dhtlist)
{
  WritablePacket *hello_p = DHTProtocol::new_dht_packet(ROUTING_KLIBS, ptype, sizeof(struct klibs_protocolheader) +
                                                        ( dhtlist->size() * sizeof(struct klibs_rt_entry) ) );
  uint8_t *payload = DHTProtocol::get_payload(hello_p);

  /*int result = */DHTProtocol::set_src(hello_p,src->data());

  struct klibs_protocolheader *header = (struct klibs_protocolheader *)payload;
  struct klibs_rt_entry *rt_entries = (struct klibs_rt_entry *)&(payload[sizeof(struct klibs_protocolheader)]);

  header->version = KLIBS_VERSION;
  header->entries = dhtlist->size();

  DHTnode *n;
  for ( int i = 0; i < dhtlist->size(); i++ ) {
    n = dhtlist->get_dhtnode(i);
    memcpy((uint8_t*)rt_entries[i].etheraddr, n->_ether_addr.data(), 6 );
    rt_entries[i].age = htonl(n->get_age_s());
  }

  return hello_p;
}

int
DHTProtocolKlibs::get_dhtnodes(Packet *p,uint8_t */*ptype*/, DHTnodelist *dhtlist)
{
//  uint16_t payload_len = DHTProtocol::get_payload_len(p);
  uint8_t *payload = DHTProtocol::get_payload(p);
  struct klibs_protocolheader *header = (struct klibs_protocolheader *)payload;
  struct klibs_rt_entry *rt_entries = (struct klibs_rt_entry *)&payload[sizeof(struct klibs_protocolheader)];

  int entries = (int)header->entries;
//  uint8_t version = header->version;

  DHTnode *node;

  for ( int i = 0; i < entries; i++ )
  {
    node = new DHTnode(EtherAddress(rt_entries[i].etheraddr));
    node->_age = Timestamp::now() - Timestamp(ntohl(rt_entries[i].age));
    dhtlist->add_dhtnode(node);
  }

  return entries;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
ELEMENT_PROVIDES(DHTProtocolKlibs)
