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

/**
 * Funtions for DHTOperation packets (creation, forward,...)
 */

WritablePacket *
DHTProtocolStorageSimple::new_dht_operation_packet(DHTOperation *op, DHTnode *src, EtherAddress *dst, bool _add_node_id)
{
  WritablePacket *p;

  if ( _add_node_id ) {
    p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, op->length() + sizeof(struct dht_simple_storage_node_info));
    op->serialize_buffer(&((DHTProtocol::get_payload(p))[sizeof(struct dht_simple_storage_node_info)]),op->length());
    struct dht_simple_storage_node_info *ni = (struct dht_simple_storage_node_info *)DHTProtocol::get_payload(p);
    src->get_nodeid((md5_byte_t *)ni->src_id, &(ni->src_id_size));
    ni->reserved = 0;
  } else {
    p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, op->length());
    op->serialize_buffer(DHTProtocol::get_payload(p),op->length());
  }

  DHTProtocol::set_src(p, op->src_of_operation.data());
  p = DHTProtocol::push_brn_ether_header(p,&(src->_ether_addr), dst, BRN_PORT_DHTSTORAGE);

  return p;
}

struct dht_simple_storage_node_info *
DHTProtocolStorageSimple::unpack_dht_operation_packet(Packet *packet, DHTOperation *_op, EtherAddress *src, bool _add_node_id)
{
  struct dht_simple_storage_node_info *ni = NULL;
  if ( _add_node_id ) {
    _op->unserialize(&((DHTProtocol::get_payload(packet))[sizeof(struct dht_simple_storage_node_info)]),DHTProtocol::get_payload_len(packet) - sizeof(struct dht_simple_storage_node_info));

    *src = EtherAddress(DHTProtocol::get_src_data(packet));
    ni = (struct dht_simple_storage_node_info *)DHTProtocol::get_payload(packet);

    _op->set_src_address_of_operation(DHTProtocol::get_src_data(packet));
  } else {
    _op->unserialize(DHTProtocol::get_payload(packet),DHTProtocol::get_payload_len(packet));
    _op->set_src_address_of_operation(DHTProtocol::get_src_data(packet));
  }

  return ni;
}

void
DHTProtocolStorageSimple::inc_hops_of_dht_operation_packet(Packet *packet, bool _add_node_id)
{
  if ( _add_node_id )
    DHTOperation::inc_hops_in_header(&((DHTProtocol::get_payload(packet))[sizeof(struct dht_simple_storage_node_info)]),
                                        DHTProtocol::get_payload_len(packet) - sizeof(struct dht_simple_storage_node_info));
  else
    DHTOperation::inc_hops_in_header(DHTProtocol::get_payload(packet),DHTProtocol::get_payload_len(packet));
}


/**
 * Functions for handling packets during data transfer coused by routing-table update
 */

WritablePacket *
DHTProtocolStorageSimple::new_data_packet(EtherAddress *src, int32_t moveID, uint8_t countRows, uint16_t data_size)
{
  WritablePacket *data_p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MOVEDDATA,
                                                        data_size + sizeof(struct dht_simple_storage_data));

  struct dht_simple_storage_data *dssd = (struct dht_simple_storage_data *)DHTProtocol::get_payload(data_p);
  dssd->count_rows = countRows;
  dssd->reserved = 0;
  dssd->move_id = htonl(moveID);

  DHTProtocol::set_src(data_p, src->data());

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
