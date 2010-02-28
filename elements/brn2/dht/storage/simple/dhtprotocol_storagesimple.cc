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

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "dhtprotocol_storagesimple.hh"

CLICK_DECLS

WritablePacket *
DHTProtocolStorageSimple::new_data_packet(EtherAddress *src, int32_t moveID, uint8_t countRows, uint16_t data_size)
{
  WritablePacket *data_p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MOVEDDATA,
                                                        data_size + sizeof(struct dht_simple_storage_data));

  struct dht_simple_storage_data *dssd = (struct dht_simple_storage_data *)DHTProtocol::get_payload(data_p);
  dssd->count_rows = countRows;
  dssd->reserved = 0;
  dssd->move_id = htonl(moveID);
  memcpy(dssd->etheraddr, src->data(),6);

  return(data_p);
}

uint8_t *
DHTProtocolStorageSimple::get_data_packet_payload(Packet *p)
{
  return &(DHTProtocol::get_payload(p)[sizeof(struct dht_simple_storage_data)]);
}

struct dht_simple_storage_data *
DHTProtocolStorageSimple::get_data_packet_header(Packet *p)
{
  return (struct dht_simple_storage_data *)DHTProtocol::get_payload(p);
}

WritablePacket *
DHTProtocolStorageSimple::new_ack_data_packet(int32_t moveID)
{
  WritablePacket *data_p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_ACKDATA, sizeof(moveID));

  uint32_t *idp = (uint32_t*)DHTProtocol::get_payload(data_p);
  *idp = htonl(moveID);

  return(data_p);
}

uint32_t
DHTProtocolStorageSimple::get_ack_movid(Packet *p)
{
  uint32_t *idp = (uint32_t *)DHTProtocol::get_payload(p);
  return (ntohl(*idp));
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
ELEMENT_PROVIDES(DHTProtocolStorageSimple)
