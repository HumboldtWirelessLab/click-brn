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
#include "elements/brn/common.hh"

#include "dhtcommunication.hh"

CLICK_DECLS
namespace DHTPacketUtil {

WritablePacket *new_dht_packet()
{
  WritablePacket *new_packet = NULL;
  struct dht_packet_header *dht_header = NULL;

  new_packet = WritablePacket::make( sizeof(struct dht_packet_header));
  dht_header = (struct dht_packet_header *)new_packet->data();

  memset(dht_header, 0, sizeof(struct dht_packet_header));

  dht_header->payload_len = 0;

  return(new_packet);	
}


WritablePacket * add_payload_list(Packet *dht_packet,struct dht_packet_payload *payload_list, int payload_list_len)
{
  int offset;
  int new_len;

  WritablePacket *new_dht_packet;

  struct dht_packet_header *dht_header;
  int i;

  uint8_t *dht_payload;

	
  for( new_len = 0,i = 0; i < payload_list_len; i++)
    new_len += payload_list[i].len + DHT_PAYLOAD_OVERHEAD;


  new_dht_packet = dht_packet->put( new_len );

  dht_header = (struct dht_packet_header *)new_dht_packet->data();

  dht_payload = (uint8_t *)new_dht_packet->data();
  dht_payload = &dht_payload[sizeof(struct dht_packet_header)];

  offset = dht_header->payload_len;
	
  for( i = 0; i < payload_list_len; i++)
  {
    memcpy(&dht_payload[offset],&payload_list[i].number ,sizeof(uint8_t));
    memcpy(&dht_payload[offset + 1],&payload_list[i].type ,sizeof(uint8_t));
    memcpy(&dht_payload[offset + 2],&payload_list[i].len ,sizeof(uint8_t));
    memcpy(&dht_payload[offset + DHT_PAYLOAD_OVERHEAD],payload_list[i].data, payload_list[i].len );
		
    offset = offset + payload_list[i].len + DHT_PAYLOAD_OVERHEAD;
  }
	
  dht_header->payload_len += new_len;

  return(new_dht_packet);
}

WritablePacket * add_payload(Packet *dht_packet, uint8_t number, uint8_t type, uint8_t len, uint8_t* data)
{
  WritablePacket *new_dht_packet;

  struct dht_packet_header *dht_header;

  uint8_t *dht_payload;

  uint16_t dht_key_end;

  new_dht_packet = dht_packet->put( len + DHT_PAYLOAD_OVERHEAD );

  dht_header =  (struct dht_packet_header *)new_dht_packet->data();
  dht_payload = (uint8_t *)new_dht_packet->data();
  dht_payload = &dht_payload[sizeof(struct dht_packet_header)];

  memcpy(&dht_payload[dht_header->payload_len],&number ,sizeof(uint8_t));
  memcpy(&dht_payload[dht_header->payload_len + 1],&type ,sizeof(uint8_t));
  memcpy(&dht_payload[dht_header->payload_len + 2],&len ,sizeof(uint8_t));
  memcpy(&dht_payload[dht_header->payload_len + DHT_PAYLOAD_OVERHEAD],data, len );

  dht_header->payload_len += len + DHT_PAYLOAD_OVERHEAD;

  dht_key_end = dht_payload[2] + DHT_PAYLOAD_OVERHEAD + 1; //jump over key and overhead of key

  if ( (dht_header->payload_len > dht_key_end) && dht_payload[dht_key_end] == TYPE_SUB_LIST )
  {
//    click_chatter("Found subkey");
    dht_payload[dht_key_end + 1] += len + DHT_PAYLOAD_OVERHEAD;
  }
  /*else
  {
    click_chatter("No Subkey");
  }*/
  return(new_dht_packet);

}

WritablePacket * add_sub_list(Packet *dht_packet, uint8_t number)
{
  WritablePacket *new_dht_packet;

  struct dht_packet_header *dht_header;

  uint8_t *dht_payload;

  new_dht_packet = dht_packet->put( DHT_PAYLOAD_OVERHEAD );

  dht_header =  (struct dht_packet_header *)new_dht_packet->data();
  dht_payload = (uint8_t *)new_dht_packet->data();
  dht_payload = &dht_payload[sizeof(struct dht_packet_header)];

  memcpy(&dht_payload[dht_header->payload_len],&number ,sizeof(uint8_t));
  dht_payload[dht_header->payload_len + 1] = TYPE_SUB_LIST;
  dht_payload[dht_header->payload_len + 2] = 0;

  dht_header->payload_len += DHT_PAYLOAD_OVERHEAD;

//  click_chatter("Add Subkey");

  return(new_dht_packet);

}


int set_header(Packet *dht_packet, uint8_t sender, uint8_t receiver, uint8_t flags, uint8_t code, uint8_t id )
{
  struct dht_packet_header *dht_header;

  dht_header =  (struct dht_packet_header *)dht_packet->data();
	
  dht_header->sender = sender;
  dht_header->receiver = receiver;
  dht_header->flags = flags;
  dht_header->code = code;
  dht_header->id = id;

  return(0);
}

int count_values(Packet *dht_packet)
{
  uint32_t val;
  uint32_t i;
  val = 0;

  struct dht_packet_header *dht_header;
  struct dht_packet_payload *payload_value;

  uint8_t *dht_payload;


  dht_header =  (struct dht_packet_header *)dht_packet->data();
  dht_payload = (uint8_t *)dht_packet->data();
  dht_payload = &dht_payload[sizeof(struct dht_packet_header)];

  i = 0;
  val = 0;

  while ( i < dht_header->payload_len )
  {
    val++;
    payload_value = (struct dht_packet_payload *)&( dht_payload[i] );
    i += payload_value->len + DHT_PAYLOAD_OVERHEAD;
  }

  return( val );
	
}

struct dht_packet_payload *get_payload_by_number(Packet *dht_packet, uint8_t num)
{
  uint32_t i;

  struct dht_packet_header *dht_header;
  struct dht_packet_payload *payload_value;

  uint8_t *dht_payload;

  dht_header = (struct dht_packet_header *)dht_packet->data();
  dht_payload = (uint8_t *)dht_packet->data();
  dht_payload = &dht_payload[sizeof(struct dht_packet_header)];

  i = 0;
 
  while ( i < dht_header->payload_len )
  {
    payload_value = (struct dht_packet_payload *)&( dht_payload[i] );

    if ( payload_value->number == num )
      return (payload_value);

    i += payload_value->len + DHT_PAYLOAD_OVERHEAD;
  }

  return( NULL );
}

int dht_unpack_payload(Packet *dht_packet)
{
  struct dht_packet_header *dht_header;
  struct dht_packet_payload *payload_value;

  uint8_t *dht_payload;

  dht_header =  (struct dht_packet_header *)dht_packet->data();
  dht_payload = (uint8_t *)dht_packet->data();
  dht_payload = &dht_payload[sizeof(struct dht_packet_header)];

  struct timeval _time_now;
  click_gettimeofday(&_time_now);
  uint32_t end_time;
  uint32_t *rel_time_p;
  uint32_t rel_time;

  int i = 0;
 

  while ( i < dht_header->payload_len )
  {
    payload_value = (struct dht_packet_payload *)&( dht_payload[i] );

    if ( payload_value->type == TYPE_REL_TIME )
    {
      rel_time_p = (uint32_t *)&dht_payload[i + DHT_PAYLOAD_OVERHEAD];
      rel_time = *rel_time_p;
      rel_time = ntohl(rel_time);
      end_time = _time_now.tv_sec + rel_time;
     // click_chatter("DHTUtils: Diff: %d  Time: %d  lokal end: %d",rel_time,_time_now.tv_sec,end_time);
      memcpy(&dht_payload[i + DHT_PAYLOAD_OVERHEAD],(void*)&end_time,4);
    }

    if ( payload_value->type == TYPE_SUB_LIST )
       i += DHT_PAYLOAD_OVERHEAD;
    else
       i += payload_value->len + DHT_PAYLOAD_OVERHEAD;
  }

  return(0);
}

int dht_unpack_payload(uint8_t *new_dht_payload, int len)
{
  struct dht_packet_payload *payload_value;

  uint8_t *dht_payload;

  dht_payload = (uint8_t *)new_dht_payload;

  struct timeval _time_now;
  click_gettimeofday(&_time_now);
  uint32_t end_time;
  uint32_t *rel_time_p;
  uint32_t rel_time;

  int i = 0;
 
  while ( i < len )
  {
    payload_value = (struct dht_packet_payload *)&( dht_payload[i] );

    if ( payload_value->type == TYPE_REL_TIME )
    {
      rel_time_p = (uint32_t *)&dht_payload[i + DHT_PAYLOAD_OVERHEAD];
      rel_time = *rel_time_p;
      rel_time = ntohl(rel_time);
      end_time = _time_now.tv_sec + rel_time;
      //click_chatter("DHTUtils: Diff: %d  Time: %d  lokal end: %d",rel_time,_time_now.tv_sec,end_time);
      memcpy(&dht_payload[i + DHT_PAYLOAD_OVERHEAD],(void*)&end_time,4);
    }

    if ( payload_value->type == TYPE_SUB_LIST )
       i += DHT_PAYLOAD_OVERHEAD;
    else
       i += payload_value->len + DHT_PAYLOAD_OVERHEAD;
  }

  return(0);
}


int dht_pack_payload(Packet *dht_packet)
{
  struct dht_packet_header *dht_header;
  struct dht_packet_payload *payload_value;

  uint8_t *dht_payload;

  dht_header =  (struct dht_packet_header *)dht_packet->data();
  dht_payload = (uint8_t *)dht_packet->data();
  dht_payload = &dht_payload[sizeof(struct dht_packet_header)];

  struct timeval _time_now;
  click_gettimeofday(&_time_now);
  uint32_t rel_time;
  uint32_t *end_time_p;
  uint32_t end_time;

  int i = 0;
 
  while ( i < dht_header->payload_len )
  {
    payload_value = (struct dht_packet_payload *)&( dht_payload[i] );

    if ( payload_value->type == TYPE_REL_TIME )
    {
      end_time_p = (uint32_t *)&dht_payload[i + DHT_PAYLOAD_OVERHEAD];
      end_time = *end_time_p;
      rel_time = end_time - _time_now.tv_sec;
      rel_time = htonl(rel_time);
      //click_chatter("DHTUtils: lokal End: %d  Time: %d  Diff: %d",end_time,_time_now.tv_sec,ntohl(rel_time));
      memcpy(&dht_payload[i + DHT_PAYLOAD_OVERHEAD],(void*)&rel_time,4);
    }

    if ( payload_value->type == TYPE_SUB_LIST )
       i += DHT_PAYLOAD_OVERHEAD;
    else
       i += payload_value->len + DHT_PAYLOAD_OVERHEAD;
  }

  return(0);
}

int dht_pack_payload(uint8_t *new_dht_payload, int len)
{
  struct dht_packet_payload *payload_value;

  uint8_t *dht_payload;

  dht_payload = (uint8_t *)new_dht_payload;

  struct timeval _time_now;
  click_gettimeofday(&_time_now);
  uint32_t end_time;
  uint32_t *end_time_p;
  uint32_t rel_time;

  int i = 0;
 
  while ( i < len )
  {
    payload_value = (struct dht_packet_payload *)&( dht_payload[i] );

    if ( payload_value->type == TYPE_REL_TIME )
    {
      end_time_p = (uint32_t *)&dht_payload[i + DHT_PAYLOAD_OVERHEAD];
      end_time = *end_time_p;
      rel_time = end_time - _time_now.tv_sec;
      rel_time = htonl(rel_time);
      //click_chatter("DHTUtils: lokal End: %d  Time: %d  Diff: %d",end_time,_time_now.tv_sec,ntohl(rel_time));
      memcpy(&dht_payload[i + DHT_PAYLOAD_OVERHEAD],(void*)&rel_time,4);
    }

    if ( payload_value->type == TYPE_SUB_LIST )
       i += DHT_PAYLOAD_OVERHEAD;
    else
       i += payload_value->len + DHT_PAYLOAD_OVERHEAD;
  }

  return(0);
}


}

ELEMENT_PROVIDES(dht_packet_util)

CLICK_ENDDECLS
