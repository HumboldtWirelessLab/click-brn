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
#ifndef DHT_PROTOCOL_DART_HH
#define DHT_PROTOCOL_DART_HH

#include <click/element.hh>
#include <click/timer.hh>
#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"

CLICK_DECLS

#define DART_MINOR_REQUEST_ID 1  /*new node to existing node*/
#define DART_MINOR_ASSIGN_ID  2  /*existing node to new node*/
#define DART_MINOR_REVOKE_ID  3  /*"Parent node" to "child node"*/
#define DART_MINOR_UPDATE_ID  4  /*to neighbouring node*/
#define DART_MINOR_RELEASE_ID 5  /*to neighbouring node*/

/** Reasons for revoke or update ID */
//#define DART_REVOKE_REASON

#define DART_ASSIGN_OK                 0
#define DART_ASSIGN_REJECT_REASON_FULL 1

struct dht_dart_lp_node_entry {
  uint8_t  status;
  uint8_t  id_size;
  uint8_t  etheraddr[6];
  uint8_t  id[MD5_DIGEST_LENGTH];
  uint32_t time;   /* seconds since 1.1.1970 */ //previous: unsigned long
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct dht_dart_routing {
  uint8_t  status;
  uint8_t  reserved;
  uint8_t  src_id_size;
  uint8_t  dst_id_size;

  uint8_t  src_id[MD5_DIGEST_LENGTH];
  uint8_t  dst_id[MD5_DIGEST_LENGTH];
} CLICK_SIZE_PACKED_ATTRIBUTE;


class DHTProtocolDart {

 public:

  static int pack_lp(uint8_t *buffer, int32_t buffer_len, DHTnode *me, DHTnodelist *nodes);
  static int unpack_lp(uint8_t *buffer, int32_t buffer_len, DHTnode *first, DHTnodelist *nodes);
  static int32_t max_no_nodes_in_lp(int32_t buffer_len);
  static void get_info(Packet *p, DHTnode *src, DHTnode *node, uint8_t *status);
  static void get_info(Packet *p, DHTnode *src, uint8_t *status);
  static WritablePacket *new_dart_nodeid_packet( DHTnode *src, DHTnode *dst, int type, Packet *p);
  static WritablePacket *new_nodeid_request_packet( DHTnode *src, DHTnode *dst);
  static WritablePacket *new_nodeid_assign_packet( DHTnode *src, DHTnode *dst, Packet *p);
};

CLICK_ENDDECLS
#endif
