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

#ifndef DHCP_H
#define DHCP_H

CLICK_DECLS
/*
 * <dhcp.h> -- our own definition of the DHCP header
 * based on a file from one of the Internet Systems Consortium, see ``http://www.isc.org''
 */
#define DHCP_UDP_OVERHEAD	( 14 + /* Ethernet header */		\
                            20 + /* IP header */			\
                             8 )   /* UDP header */

#define DHCP_SNAME_LEN			64
#define DHCP_FILE_LEN				128
#define DHCP_FIXED_NON_UDP	236
#define DHCP_FIXED_LEN		(DHCP_FIXED_NON_UDP + DHCP_UDP_OVERHEAD)

						/* Everything but options. */

#define DHCP_MTU_MAX		1000

#define DHCP_OPTION_LEN	(DHCP_MTU_MAX - DHCP_FIXED_LEN)
#define DHCP_MIN_OPTION_LEN 312

#define BOOTP_MIN_LEN		300
#define DHCP_MIN_LEN    548


/* BOOTP message */
#define BOOTREQUEST     1 /* client to server */
#define BOOTREPLY       2 /* server to client */

/* Possible values for flags field... */
#define BOOTP_BROADCAST 32768L

/* Possible values for hardware type (htype) field... */
#define HTYPE_ETHER     1               /* Ethernet 10Mbps              */
#define HTYPE_IEEE802   6               /* IEEE 802.2 Token Ring...     */
#define HTYPE_FDDI      8               /* FDDI...                      */


/* Magic cookie validating dhcp options field (and bootp vendor
 *    extensions field). */
#define DHCP_OPTIONS_COOKIE     "\143\202\123\143"

#define DHCP_OPTIONS_COOKIE_LEN	4
/* DHCP Option codes: */

/* maximum length of a single option */
#define MAX_OPTION_LEN 255


#define DHO_PAD			0
#define DHO_SUBNET_MASK		1
#define DHO_TIME_OFFSET		2
#define DHO_ROUTERS			3
#define DHO_TIME_SERVERS		4
#define DHO_NAME_SERVERS		5
#define DHO_DOMAIN_NAME_SERVERS	6
#define DHO_LOG_SERVERS		7
#define DHO_COOKIE_SERVERS		8
#define DHO_LPR_SERVERS		9
#define DHO_IMPRESS_SERVERS		10
#define DHO_RESOURCE_LOCATION_SERVERS	11
#define DHO_HOST_NAME		12
#define DHO_BOOT_SIZE		13
#define DHO_MERIT_DUMP		14
#define DHO_DOMAIN_NAME		15
#define DHO_SWAP_SERVER		16
#define DHO_ROOT_PATH		17
#define DHO_EXTENSIONS_PATH		18
#define DHO_IP_FORWARDING		19
#define DHO_NON_LOCAL_SOURCE_ROUTING	20
#define DHO_POLICY_FILTER		21
#define DHO_MAX_DGRAM_REASSEMBLY	22
#define DHO_DEFAULT_IP_TTL		23
#define DHO_PATH_MTU_AGING_TIMEOUT	24
#define DHO_PATH_MTU_PLATEAU_TABLE	25
#define DHO_INTERFACE_MTU		26
#define DHO_ALL_SUBNETS_LOCAL		27
#define DHO_BROADCAST_ADDRESS		28
#define DHO_PERFORM_MASK_DISCOVERY	29
#define DHO_MASK_SUPPLIER		30
#define DHO_ROUTER_DISCOVERY		31
#define DHO_ROUTER_SOLICITATION_ADDRESS	32
#define DHO_STATIC_ROUTES		33
#define DHO_TRAILER_ENCAPSULATION	34
#define DHO_ARP_CACHE_TIMEOUT		35
#define DHO_IEEE802_3_ENCAPSULATION	36
#define DHO_DEFAULT_TCP_TTL		37
#define DHO_TCP_KEEPALIVE_INTERVAL	38
#define DHO_TCP_KEEPALIVE_GARBAGE	39
#define DHO_NIS_DOMAIN		40
#define DHO_NIS_SERVERS		41
#define DHO_NTP_SERVERS		42
#define DHO_VENDOR_ENCAPSULATED_OPTIONS	43
#define DHO_NETBIOS_NAME_SERVERS	44
#define DHO_NETBIOS_DD_SERVER		45
#define DHO_NETBIOS_NODE_TYPE		46
#define DHO_NETBIOS_SCOPE		47
#define DHO_FONT_SERVERS		48
#define DHO_X_DISPLAY_MANAGER		49
#define DHO_DHCP_REQUESTED_ADDRESS	50
#define DHO_DHCP_LEASE_TIME		51
#define DHO_DHCP_OPTION_OVERLOAD	52
#define DHO_DHCP_MESSAGE_TYPE		53
#define DHO_DHCP_SERVER_IDENTIFIER	54
#define DHO_DHCP_PARAMETER_REQUEST_LIST	55
#define DHO_DHCP_MESSAGE		56
#define DHO_DHCP_MAX_MESSAGE_SIZE	57
#define DHO_DHCP_RENEWAL_TIME		58
#define DHO_DHCP_REBINDING_TIME	59
#define DHO_VENDOR_CLASS_IDENTIFIER	60
#define DHO_DHCP_CLIENT_IDENTIFIER	61
#define DHO_NWIP_DOMAIN_NAME		62
#define DHO_NWIP_SUBOPTIONS		63
#define DHO_USER_CLASS		77
#define DHO_FQDN			81
#define DHO_DHCP_AGENT_OPTIONS	82
#define DHO_SUBNET_SELECTION		118 /* RFC3011! */
/* The DHO_AUTHENTICATE option is not a standard yet, so I've
   allocated an option out of the "local" option space for it on a
   temporary basis.  Once an option code number is assigned, I will
   immediately and shamelessly break this, so don't count on it
   continuing to work. */
#define DHO_AUTHENTICATE		210

#define DHO_END			255


/* DHCP message types. */
#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPDECLINE  4
#define DHCPACK      5
#define DHCPNAK      6
#define DHCPRELEASE  7
#define DHCPINFORM   8

/* DHCP client status */
#define DHCPINIT       0
#define DHCPINITREBOOT 1
#define DHCPSELECTING  2
#define DHCPBOUND      3
#define DHCPRENEWING   4
#define DHCPREBINDING  5


struct iaddr {
  unsigned len;
  unsigned char iabuf [16];
};

/*
enum DHCP_VERSION {
  BOOTP,
  DHCP,
  DHCPV6
};
*/

#define DHCP_OPTION_OVERHEAD 2

struct dhcp_option
{
  uint8_t	code;
  uint8_t	length;
  uint8_t	data[MAX_OPTION_LEN];
};

struct dhcp_packet
{
  uint8_t  op;    /* 0: Message opcode/type */
  uint8_t  htype; /* 1: Hardware addr type (net/if_types.h) */
  uint8_t  hlen;  /* 2: Hardware addr length */
  uint8_t  hops;  /* 3: Number of relay agent hops from client */
  uint32_t xid;   /* 4: Transaction ID */
  uint16_t secs;  /* 8: Seconds since client started looking */
  uint16_t flags;	        /* 10: Flag bits */
  struct in_addr ciaddr;	/* 12: Client IP address (if already in use) */
  struct in_addr yiaddr;	/* 16: Client IP address */
  struct in_addr siaddr;	/* 18: IP address of next server to talk to */
  struct in_addr giaddr;	/* 20: DHCP relay agent IP address */
  unsigned char chaddr [16];	/* 24: Client hardware address */
  char sname [DHCP_SNAME_LEN];	/* 40: Server name */
  char file [DHCP_FILE_LEN];	/* 104: Boot filename */
  unsigned char options [DHCP_OPTION_LEN];  /* 212: Optional parameters (actual length dependent on MTU). */
};

CLICK_ENDDECLS
#endif
