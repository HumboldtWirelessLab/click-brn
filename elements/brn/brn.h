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

#ifndef CLICKNET_BRN_H
#define CLICKNET_BRN_H

# ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN 
#   include <windows.h>
#   include <winsock.h>
# endif //_WIN32

/* get struct in_addr */
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#if CLICK_LINUXMODULE
# include <net/checksum.h>
# include <linux/in.h>
#else
# include <sys/types.h>
# ifndef _WIN32
#   include <netinet/in.h>
# endif //_WIN32
#endif

//#define DEBUG
#ifdef DEBUG
 #define DPRINTF(x...) (fprintf(stderr,"%s:%i:%s",__FILE__, __LINE__, __FUNCTION__),fprintf(stderr, x))
#else
 #define DPRINTF 
#endif

#define ETHERTYPE_BRN          0x8086 /* Berlin Roofnet Protocol */

/* define structure of Berlin Roofnet packet (BRN) */
CLICK_SIZE_PACKED_STRUCTURE(
struct click_brn {,
    uint8_t   dst_port;
    uint8_t   src_port;
    uint16_t  body_length;
    uint8_t   ttl;
    uint8_t   tos;          ///< type of service
});

#define BRN_TOS_BE     0
#define BRN_TOS_HP     1

struct hwaddr {
  uint8_t data[6];  /* hardware address */
}; // AZu  __packed__;

#define BRN_PORT_SDP              1
#define BRN_PORT_TFTP             2
#define BRN_PORT_DSR              3
#define BRN_PORT_BCAST            4 
#define BRN_PORT_LINK_PROBE       6
#define BRN_PORT_DHT              7
#define BRN_PORT_IAPP             8
#define BRN_PORT_GATEWAY          9 
#define BRN_PORT_FLOW            16

struct click_brn_neighbor_beacon {
  uint8_t      operation; /*rq/response*/
  uint8_t      reserved[3];
  uint32_t     magic;

#define BRN_NEIGHBOR_BEACON_OP_RQ 0
#define BRN_NEIGHBOR_BEACON_OP_MSG 1
#define BRN_NEIGHBOR_BEACON_MAGIC 0xcafe0101

  union {
    struct {
      /* empty */
    } rq;
    struct {
      // current running software
      uint32_t curr_software_id;
      //uint32_t       curr_config;
      uint32_t curr_time_sec;
               uint32_t curr_time_usec;
               uint16_t curr_time_stratum;

#define BRN_MAX_LIBRARY_SIZE 8

      // available software
      struct {
        uint32_t       software_id;
        //uint32_t     config;
        uint32_t       activation_time;
      } library[BRN_MAX_LIBRARY_SIZE];
    } msg;
  } u;
};

struct click_brn_tftp {
  
#define BRN_TFTP_OP_RQ 0
#define BRN_TFTP_OP_DATA 1

  uint8_t operation;
  uint8_t reserved[3];

  union {
    struct {
            uint32_t file_id;
            uint32_t requested_packet;
    } rq;
    struct {
      uint32_t file_id;
      uint32_t requested_packet;
      uint32_t packet_length;
      uint32_t total_file_length;

#define BRN_TFTP_PACKET_SIZE 512
      uint8_t data[BRN_TFTP_PACKET_SIZE];
    } data;
  } u;  
};

struct click_brn_iapp_ho {
  uint8_t       addr_sta[6];  ///< Address of the roaming station 
  uint8_t       addr_mold[6];  ///< Address of the old mesh node
  uint8_t       addr_mnew[6];  ///< Address of the new mesh node
  uint8_t       seq_no;   ///< sequence number, with addr_mnew unique
}; 

struct click_brn_iapp_he {
  uint8_t       addr_sta[6];      ///< Address of the station 
  uint8_t       addr_ap_curr[6];  ///< Address of the current ap
  uint8_t       addr_ap_cand[6];  ///< Address of the ap candidate
  uint8_t       authoritive;      ///< Whether it is a request or response
};

struct click_brn_iapp {
  uint8_t       type;     ///< Type discriminator
  uint8_t       reserved[1]; ///< Reserved
  uint16_t      payload_len; ///< length of the payload, if any.

#define CLICK_BRN_IAPP_HON  1  ///< handover notify 
#define CLICK_BRN_IAPP_HOR  2  ///< handover reply
#define CLICK_BRN_IAPP_DAT  3  ///< handover data
#define CLICK_BRN_IAPP_HRU  4  ///< handover route update
#define CLICK_BRN_IAPP_HEL  5  ///< iapp hello

  union {
    click_brn_iapp_ho ho; ///< payload for types 1-4
    click_brn_iapp_he he; ///< payload for type 5
  } payload;

  // used for transferring additional payload during hand over
  uint8_t       payload_type;  ///< used for additional payload
  #define CLICK_BRN_IAPP_PAYLOAD_EMPTY    0  ///< no payload
  #define CLICK_BRN_IAPP_PAYLOAD_GATEWAY  1  ///< payload from gateway
};

struct click_dsr_hop {
  hwaddr  hw;
  uint16_t metric;
};

#ifdef CLICK_NS
// TODO adapt peek and all other things for max hop count != 16
# define BRN_DSR_MAX_HOP_COUNT  16
#else // CLICK_NS
// be reverse-compatible, we have already deployed nodes with this value
# define BRN_DSR_MAX_HOP_COUNT  16
#endif // CLICK_NS

/* DSR Route Request */
struct click_brn_dsr_rreq {
  uint16_t  dsr_id;
};

/* DSR Route Reply */
struct click_brn_dsr_rrep {
  uint8_t   dsr_flags;
  uint8_t   dsr_pad;
};

/* DSR Route Error */
struct click_brn_dsr_rerr {

#define BRN_DSR_RERR_TYPE_NODE_UNREACHABLE  1

  // broken link between dsr_unreachable_src and dsr_unreachable_dst
  hwaddr    dsr_unreachable_src;
  hwaddr    dsr_unreachable_dst;
  hwaddr    dsr_msg_originator; // TODO this info is implicit available in dsr_dst
  uint8_t   dsr_error;
  uint8_t   dsr_flags;
};

/* DSR Source Routed Packet */
struct click_brn_dsr_src {
  uint16_t      dsr_id;
  uint8_t       dsr_salvage;

  uint8_t       reserved; // @deprecated dsr_payload_type

#define BRN_DSR_PAYLOADTYPE_KEY 0

#define BRN_DSR_PAYLOADTYPE_RAW 0
#define BRN_DSR_PAYLOADTYPE_UDT 1
#define BRN_DSR_PAYLOADTYPE_DHT 2

#define BRN_DSR_PAYLOADTYPE_REST  10 /* for classifier's "-" argument. not a valid message type */

#define BRN_MAX_ETHER_LENGTH 1500
}; /* data */

/* Common DSR Structure */
struct click_brn_dsr {
  uint8_t       dsr_type;
  uint8_t       reserved[3];

  hwaddr        dsr_dst;
  hwaddr        dsr_src;

  /* in case of clients use their IPs */
  struct in_addr  dsr_ip_dst;
  struct in_addr  dsr_ip_src;

#define BRN_NOT_IP_NOT_AVAILABLE "0.0.0.0"
#define BRN_INTERNAL_NODE_IP "254.1.1.1"

  uint8_t       dsr_hop_count; /* total hop count */
  uint8_t       dsr_segsleft; /* hops left */
  click_dsr_hop addr[BRN_DSR_MAX_HOP_COUNT]; /* hops */

#define BRN_DSR_RREQ 1
#define BRN_DSR_RREP 2
#define BRN_DSR_RERR 3
#define BRN_DSR_SRC  4

  union {
    click_brn_dsr_rreq rreq;
    click_brn_dsr_rrep rrep;
    click_brn_dsr_rerr rerr;
    click_brn_dsr_src src; /* data */
  } body;
};

/* BRN Broadcast */
struct click_brn_bcast {
  uint16_t      bcast_id;
  hwaddr        dsr_dst;
  hwaddr        dsr_src;
};


typedef enum tagPacketType
{
  PT_HANDSHAKE = 0, ///< Handshake
  PT_KEEPALIVE = 1, ///< Keep alive
  PT_ACK       = 2, ///< Acknowledgement
  PT_NAK       = 3, ///< Negative Acknowledgement
  PT_RESERVED  = 4, ///< Reserved for congestion control
  PT_SHUTDOWN  = 5, ///< Shutdown
  PT_ACK2      = 6, ///< Acknowledgement of Acknowledgement
  PT_FFU       = 7, ///< For future use

  PT_FIRST     = 0, ///< Alias for the entry with the lowest value
  PT_LAST      = 7  ///< Alias for the entry with the highest value
} PacketType;

#define UDT_HEADER_SIZE 8

struct click_brn_dsr_udt {

  /* 32 bit packed header structure
   * bit 0 '== 0': data packet, bits 1~31 sequence number
   * bit 0 '== 1': control packet, bits 1~3 type, 
   *                               bits 4~15 reserved
   *                               bits 16~31 ack sequence number
   */
  uint32_t      packed_hdr; 
  uint32_t      timestamp;
  
  /* variable length payload */
};

#define PACKET_TYPE_KEY_USER_ANNO 10

CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
#endif
