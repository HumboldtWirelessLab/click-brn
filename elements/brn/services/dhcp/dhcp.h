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
} CLICK_SIZE_PACKED_ATTRIBUTE;

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
} CLICK_SIZE_PACKED_ATTRIBUTE;

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
} CLICK_SIZE_PACKED_ATTRIBUTE;

#define TFS(x)  (const struct true_false_string*)(x)

enum field_type {
  special,
  none,
  presence,
  ipv4,     /* single IPv4 address */
  ipv4_list,    /* list of IPv4 addresses */
  string,
  bytes,
  opaque,
  val_boolean,
  val_u_byte,
  val_u_short,
  val_u_short_list,
  val_u_le_short,
  val_u_long,
  time_in_secs,
  fqdn,
  ipv4_or_fqdn
};

struct opt_info {
  const char  *text;
  enum field_type ftype;
  const void *data;
} CLICK_SIZE_PACKED_ATTRIBUTE;

typedef struct true_false_string {
  const char    *true_string;
  const char    *false_string;
} true_false_string;


static const true_false_string toggle_tfs = { "Enabled", "Disabled" };

static const true_false_string yes_no_tfs = { "Yes", "No" };

#ifdef NEED_DHCP_MESSAGE_TYPES

static const char *message_types[] = { "Unknown", "Discover", "Offer", "Request", "Decline",
  "Ack", "Nak", "Release", "Inform" };
#define COUNT_OPTIONS 8

#endif

#ifdef NEED_DHCP_MESSAGE_OPTS

static struct opt_info opt[] = {
    /*   0 */ { "Padding",          none, NULL },
    /*   1 */ { "Subnet Mask",        ipv4, NULL },
    /*   2 */ { "Time Offset",        time_in_secs, NULL },
    /*   3 */ { "Router",         ipv4_list, NULL },
    /*   4 */ { "Time Server",        ipv4_list, NULL },
    /*   5 */ { "Name Server",        ipv4_list, NULL },
    /*   6 */ { "Domain Name Server",     ipv4_list, NULL },
    /*   7 */ { "Log Server",       ipv4_list, NULL },
    /*   8 */ { "Cookie Server",        ipv4_list, NULL },
    /*   9 */ { "LPR Server",       ipv4_list, NULL },
    /*  10 */ { "Impress Server",       ipv4_list, NULL },
    /*  11 */ { "Resource Location Server",     ipv4_list, NULL },
    /*  12 */ { "Host Name",        string, NULL },
    /*  13 */ { "Boot File Size",       val_u_short, NULL },
    /*  14 */ { "Merit Dump File",        string, NULL },
    /*  15 */ { "Domain Name",        string, NULL },
    /*  16 */ { "Swap Server",        ipv4, NULL },
    /*  17 */ { "Root Path",        string, NULL },
    /*  18 */ { "Extensions Path",        string, NULL },
    /*  19 */ { "IP Forwarding",        val_boolean, TFS(&toggle_tfs) },
    /*  20 */ { "Non-Local Source Routing",     val_boolean, TFS(&toggle_tfs) },
    /*  21 */ { "Policy Filter",        special, NULL },
    /*  22 */ { "Maximum Datagram Reassembly Size",   val_u_short, NULL },
    /*  23 */ { "Default IP Time-to-Live",      val_u_byte, NULL },
    /*  24 */ { "Path MTU Aging Timeout",     time_in_secs, NULL },
    /*  25 */ { "Path MTU Plateau Table",     val_u_short_list, NULL },
    /*  26 */ { "Interface MTU",        val_u_short, NULL },
    /*  27 */ { "All Subnets are Local",      val_boolean, TFS(&yes_no_tfs) },
    /*  28 */ { "Broadcast Address",      ipv4, NULL },
    /*  29 */ { "Perform Mask Discovery",     val_boolean, TFS(&toggle_tfs) },
    /*  30 */ { "Mask Supplier",        val_boolean, TFS(&yes_no_tfs) },
    /*  31 */ { "Perform Router Discover",      val_boolean, TFS(&toggle_tfs) },
    /*  32 */ { "Router Solicitation Address",    ipv4, NULL },
    /*  33 */ { "Static Route",       special, NULL },
    /*  34 */ { "Trailer Encapsulation",      val_boolean, TFS(&toggle_tfs) },
    /*  35 */ { "ARP Cache Timeout",      time_in_secs, NULL },
    /*  36 */ { "Ethernet Encapsulation",     val_boolean, TFS(&toggle_tfs) },
    /*  37 */ { "TCP Default TTL",        val_u_byte, NULL },
    /*  38 */ { "TCP Keepalive Interval",     time_in_secs, NULL },
    /*  39 */ { "TCP Keepalive Garbage",      val_boolean, TFS(&toggle_tfs) },
    /*  40 */ { "Network Information Service Domain", string, NULL },
    /*  41 */ { "Network Information Service Servers",  ipv4_list, NULL },
    /*  42 */ { "Network Time Protocol Servers",    ipv4_list, NULL },
    /*  43 */ { "Vendor-Specific Information",    special, NULL },
    /*  44 */ { "NetBIOS over TCP/IP Name Server",    ipv4_list, NULL },
    /*  45 */ { "NetBIOS over TCP/IP Datagram Distribution Name Server", ipv4_list, NULL },
    /*  46 */ { "NetBIOS over TCP/IP Node Type",    val_u_byte, NULL }, //VALS(nbnt_vals) },
    /*  47 */ { "NetBIOS over TCP/IP Scope",    string, NULL },
    /*  48 */ { "X Window System Font Server",    ipv4_list, NULL },
    /*  49 */ { "X Window System Display Manager",    ipv4_list, NULL },
    /*  50 */ { "Requested IP Address",     ipv4, NULL },
    /*  51 */ { "IP Address Lease Time",      time_in_secs, NULL },
    /*  52 */ { "Option Overload",        special, NULL },
    /*  53 */ { "DHCP Message Type",      special, NULL },
    /*  54 */ { "Server Identifier",      ipv4, NULL },
    /*  55 */ { "Parameter Request List",     special, NULL },
    /*  56 */ { "Message",          string, NULL },
    /*  57 */ { "Maximum DHCP Message Size",    val_u_short, NULL },
    /*  58 */ { "Renewal Time Value",     time_in_secs, NULL },
    /*  59 */ { "Rebinding Time Value",     time_in_secs, NULL },
    /*  60 */ { "Vendor class identifier",      special, NULL },
    /*  61 */ { "Client identifier",      special, NULL },
    /*  62 */ { "Novell/Netware IP domain",     string, NULL },
    /*  63 */ { "Novell Options",       special, NULL },
    /*  64 */ { "Network Information Service+ Domain",  string, NULL },
    /*  65 */ { "Network Information Service+ Servers", ipv4_list, NULL },
    /*  66 */ { "TFTP Server Name",       string, NULL },
    /*  67 */ { "Bootfile name",        string, NULL },
    /*  68 */ { "Mobile IP Home Agent",     ipv4_list, NULL },
    /*  69 */ { "SMTP Server",        ipv4_list, NULL },
    /*  70 */ { "POP3 Server",        ipv4_list, NULL },
    /*  71 */ { "NNTP Server",        ipv4_list, NULL },
    /*  72 */ { "Default WWW Server",     ipv4_list, NULL },
    /*  73 */ { "Default Finger Server",      ipv4_list, NULL },
    /*  74 */ { "Default IRC Server",     ipv4_list, NULL },
    /*  75 */ { "StreetTalk Server",      ipv4_list, NULL },
    /*  76 */ { "StreetTalk Directory Assistance Server", ipv4_list, NULL },
    /*  77 */ { "User Class Information",     opaque, NULL },
    /*  78 */ { "Directory Agent Information",    special, NULL },
    /*  79 */ { "Service Location Agent Scope",   special, NULL },
    /*  80 */ { "Naming Authority",       opaque, NULL },
    /*  81 */ { "Client Fully Qualified Domain Name", special, NULL },
    /*  82 */ { "Agent Information Option",                 special, NULL },
    /*  83 */ { "Unassigned",       opaque, NULL },
    /*  84 */ { "Unassigned",       opaque, NULL },
    /*  85 */ { "Novell Directory Services Servers",  special, NULL },
    /*  86 */ { "Novell Directory Services Tree Name",  string, NULL },
    /*  87 */ { "Novell Directory Services Context",  string, NULL },
    /*  88 */ { "IEEE 1003.1 POSIX Timezone",   opaque, NULL },
    /*  89 */ { "Fully Qualified Domain Name",    opaque, NULL },
    /*  90 */ { "Authentication",       special, NULL },
    /*  91 */ { "Vines TCP/IP Server Option",   opaque, NULL },
    /*  92 */ { "Server Selection Option",      opaque, NULL },
    /*  93 */ { "Client System Architecture",   opaque, NULL },
    /*  94 */ { "Client Network Device Interface",    opaque, NULL },
    /*  95 */ { "Lightweight Directory Access Protocol",  opaque, NULL },
    /*  96 */ { "IPv6 Transitions",       opaque, NULL },
    /*  97 */ { "UUID/GUID-based Client Identifier",  opaque, NULL },
    /*  98 */ { "Open Group's User Authentication",   opaque, NULL },
    /*  99 */ { "Unassigned",       opaque, NULL },
    /* 100 */ { "Printer Name",       opaque, NULL },
    /* 101 */ { "MDHCP multicast address",      opaque, NULL },
    /* 102 */ { "Removed/unassigned",     opaque, NULL },
    /* 103 */ { "Removed/unassigned",     opaque, NULL },
    /* 104 */ { "Removed/unassigned",     opaque, NULL },
    /* 105 */ { "Removed/unassigned",     opaque, NULL },
    /* 106 */ { "Removed/unassigned",     opaque, NULL },
    /* 107 */ { "Removed/unassigned",     opaque, NULL },
    /* 108 */ { "Swap Path Option",       opaque, NULL },
    /* 109 */ { "Unassigned",       opaque, NULL },
    /* 110 */ { "IPX Compability",        opaque, NULL },
    /* 111 */ { "Unassigned",       opaque, NULL },
    /* 112 */ { "NetInfo Parent Server Address",    ipv4_list, NULL },
    /* 113 */ { "NetInfo Parent Server Tag",    string, NULL },
    /* 114 */ { "URL",          opaque, NULL },
    /* 115 */ { "DHCP Failover Protocol",     opaque, NULL },
    /* 116 */ { "DHCP Auto-Configuration",      opaque, NULL },
    /* 117 */ { "Name Service Search",            opaque, NULL },
    /* 118 */ { "Subnet Selection Option",            opaque, NULL },
    /* 119 */ { "Domain Search",        opaque, NULL },
    /* 120 */ { "SIP Servers",        opaque, NULL },
    /* 121 */ { "Classless Static Route",           opaque, NULL },
    /* 122 */ { "CableLabs Client Configuration",   opaque, NULL },
    /* 123 */ { "Unassigned",       opaque, NULL },
    /* 124 */ { "Unassigned",       opaque, NULL },
    /* 125 */ { "Unassigned",       opaque, NULL },
    /* 126 */ { "Extension",        opaque, NULL },
    /* 127 */ { "Extension",        opaque, NULL },
    /* 128 */ { "Private",          opaque, NULL },
    /* 129 */ { "Private",          opaque, NULL },
    /* 130 */ { "Private",          opaque, NULL },
    /* 131 */ { "Private",          opaque, NULL },
    /* 132 */ { "Private",          opaque, NULL },
    /* 133 */ { "Private",          opaque, NULL },
    /* 134 */ { "Private",          opaque, NULL },
    /* 135 */ { "Private",          opaque, NULL },
    /* 136 */ { "Private",          opaque, NULL },
    /* 137 */ { "Private",          opaque, NULL },
    /* 138 */ { "Private",          opaque, NULL },
    /* 139 */ { "Private",          opaque, NULL },
    /* 140 */ { "Private",          opaque, NULL },
    /* 141 */ { "Private",          opaque, NULL },
    /* 142 */ { "Private",          opaque, NULL },
    /* 143 */ { "Private",          opaque, NULL },
    /* 144 */ { "Private",          opaque, NULL },
    /* 145 */ { "Private",          opaque, NULL },
    /* 146 */ { "Private",          opaque, NULL },
    /* 147 */ { "Private",          opaque, NULL },
    /* 148 */ { "Private",          opaque, NULL },
    /* 149 */ { "Private",          opaque, NULL },
    /* 150 */ { "Private",          opaque, NULL },
    /* 151 */ { "Private",          opaque, NULL },
    /* 152 */ { "Private",          opaque, NULL },
    /* 153 */ { "Private",          opaque, NULL },
    /* 154 */ { "Private",          opaque, NULL },
    /* 155 */ { "Private",          opaque, NULL },
    /* 156 */ { "Private",          opaque, NULL },
    /* 157 */ { "Private",          opaque, NULL },
    /* 158 */ { "Private",          opaque, NULL },
    /* 159 */ { "Private",          opaque, NULL },
    /* 160 */ { "Private",          opaque, NULL },
    /* 161 */ { "Private",          opaque, NULL },
    /* 162 */ { "Private",          opaque, NULL },
    /* 163 */ { "Private",          opaque, NULL },
    /* 164 */ { "Private",          opaque, NULL },
    /* 165 */ { "Private",          opaque, NULL },
    /* 166 */ { "Private",          opaque, NULL },
    /* 167 */ { "Private",          opaque, NULL },
    /* 168 */ { "Private",          opaque, NULL },
    /* 169 */ { "Private",          opaque, NULL },
    /* 170 */ { "Private",          opaque, NULL },
    /* 171 */ { "Private",          opaque, NULL },
    /* 172 */ { "Private",          opaque, NULL },
    /* 173 */ { "Private",          opaque, NULL },
    /* 174 */ { "Private",          opaque, NULL },
    /* 175 */ { "Private",          opaque, NULL },
    /* 176 */ { "Private",          opaque, NULL },
    /* 177 */ { "Private",          opaque, NULL },
    /* 178 */ { "Private",          opaque, NULL },
    /* 179 */ { "Private",          opaque, NULL },
    /* 180 */ { "Private",          opaque, NULL },
    /* 181 */ { "Private",          opaque, NULL },
    /* 182 */ { "Private",          opaque, NULL },
    /* 183 */ { "Private",          opaque, NULL },
    /* 184 */ { "Private",          opaque, NULL },
    /* 185 */ { "Private",          opaque, NULL },
    /* 186 */ { "Private",          opaque, NULL },
    /* 187 */ { "Private",          opaque, NULL },
    /* 188 */ { "Private",          opaque, NULL },
    /* 189 */ { "Private",          opaque, NULL },
    /* 190 */ { "Private",          opaque, NULL },
    /* 191 */ { "Private",          opaque, NULL },
    /* 192 */ { "Private",          opaque, NULL },
    /* 193 */ { "Private",          opaque, NULL },
    /* 194 */ { "Private",          opaque, NULL },
    /* 195 */ { "Private",          opaque, NULL },
    /* 196 */ { "Private",          opaque, NULL },
    /* 197 */ { "Private",          opaque, NULL },
    /* 198 */ { "Private",          opaque, NULL },
    /* 199 */ { "Private",          opaque, NULL },
    /* 200 */ { "Private",          opaque, NULL },
    /* 201 */ { "Private",          opaque, NULL },
    /* 202 */ { "Private",          opaque, NULL },
    /* 203 */ { "Private",          opaque, NULL },
    /* 204 */ { "Private",          opaque, NULL },
    /* 205 */ { "Private",          opaque, NULL },
    /* 206 */ { "Private",          opaque, NULL },
    /* 207 */ { "Private",          opaque, NULL },
    /* 208 */ { "Private",          opaque, NULL },
    /* 209 */ { "Private",          opaque, NULL },
    /* 210 */ { "Authentication",   special, NULL },
    /* 211 */ { "Private",          opaque, NULL },
    /* 212 */ { "Private",          opaque, NULL },
    /* 213 */ { "Private",          opaque, NULL },
    /* 214 */ { "Private",          opaque, NULL },
    /* 215 */ { "Private",          opaque, NULL },
    /* 216 */ { "Private",          opaque, NULL },
    /* 217 */ { "Private",          opaque, NULL },
    /* 218 */ { "Private",          opaque, NULL },
    /* 219 */ { "Private",          opaque, NULL },
    /* 220 */ { "Private",          opaque, NULL },
    /* 221 */ { "Private",          opaque, NULL },
    /* 222 */ { "Private",          opaque, NULL },
    /* 223 */ { "Private",          opaque, NULL },
    /* 224 */ { "Private",          opaque, NULL },
    /* 225 */ { "Private",          opaque, NULL },
    /* 226 */ { "Private",          opaque, NULL },
    /* 227 */ { "Private",          opaque, NULL },
    /* 228 */ { "Private",          opaque, NULL },
    /* 229 */ { "Private",          opaque, NULL },
    /* 230 */ { "Private",          opaque, NULL },
    /* 231 */ { "Private",          opaque, NULL },
    /* 232 */ { "Private",          opaque, NULL },
    /* 233 */ { "Private",          opaque, NULL },
    /* 234 */ { "Private",          opaque, NULL },
    /* 235 */ { "Private",          opaque, NULL },
    /* 236 */ { "Private",          opaque, NULL },
    /* 237 */ { "Private",          opaque, NULL },
    /* 238 */ { "Private",          opaque, NULL },
    /* 239 */ { "Private",          opaque, NULL },
    /* 240 */ { "Private",          opaque, NULL },
    /* 241 */ { "Private",          opaque, NULL },
    /* 242 */ { "Private",          opaque, NULL },
    /* 243 */ { "Private",          opaque, NULL },
    /* 244 */ { "Private",          opaque, NULL },
    /* 245 */ { "Private",          opaque, NULL },
    /* 246 */ { "Private",          opaque, NULL },
    /* 247 */ { "Private",          opaque, NULL },
    /* 248 */ { "Private",          opaque, NULL },
    /* 249 */ { "Private",          opaque, NULL },
    /* 250 */ { "Private",          opaque, NULL },
    /* 251 */ { "Private",          opaque, NULL },
    /* 252 */ { "Private",          opaque, NULL },
    /* 253 */ { "Private",          opaque, NULL },
    /* 254 */ { "Private",          opaque, NULL },
    /* 255 */ { "End",          opaque, NULL }
};

#endif

CLICK_ENDDECLS
#endif
