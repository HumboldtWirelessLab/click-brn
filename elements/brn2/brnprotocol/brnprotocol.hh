#ifndef CLICK_BRNPROTOCOL_HH
#define CLICK_BRNPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS
/**
TODO: use brn-header ttl instead of hops
*/


#define ETHERTYPE_BRN        0x8086 /* Berlin Roofnet Protocol */
#define BRN_MAX_ETHER_LENGTH 1500

/* define structure of Berlin Roofnet packet (BRN) */
CLICK_SIZE_PACKED_STRUCTURE(
struct click_brn {,
  uint8_t   dst_port;
  uint8_t   src_port;
  uint16_t  body_length;
  uint8_t   ttl;
  uint8_t   tos;          ///< type of service
});

typedef struct click_brn brn_header;

#define BRN_TOS_BE     0
#define BRN_TOS_LP     1
#define BRN_TOS_MP     2
#define BRN_TOS_HP     3
#define BRN_TOS_BOP_0  4
#define BRN_TOS_BOP_1  5
#define BRN_TOS_BOP_2  6
#define BRN_TOS_BOP_3  7



struct hwaddr {
  uint8_t data[6];  /* hardware address */
}; // AZu  __packed__;

struct netaddr {
  uint8_t data[4];  /* hardware address */
}; // AZu  __packed__;

union addr {
  struct hwaddr _hwaddr;
  struct netaddr _netaddr;
};

/*Basics and Services*/
#define BRN_PORT_LINK_PROBE                1 /*0x01*/
#define BRN_PORT_IAPP                      2 /*0x02*/
#define BRN_PORT_GATEWAY                   3 /*0x03*/
#define BRN_PORT_EVENTHANDLER              4 /*0x04*/
#define BRN_PORT_ALARMINGPROTOCOL          5 /*0x05*/
#define BRN_PORT_COMP_ALARMINGPROTOCOL     6 /*0x06*/
/*Routing*/
#define BRN_PORT_DSR                      10 /*0x0a*/
#define BRN_PORT_BCASTROUTING             11 /*0x0b*/
#define BRN_PORT_FLOODING                 12 /*0x0c*/
#define BRN_PORT_BATMAN                   13 /*0x0d*/
#define BRN_PORT_GEOROUTING               14 /*0x0e*/
#define BRN_PORT_DART                     15 /*0x0f*/
#define BRN_PORT_HAWK                     16 /*0x10*/
#define BRN_PORT_OLSR                     17 /*0x11*/
#define BRN_PORT_AODV                     18 /*0x12*/
/*Clustering*/
#define BRN_PORT_DCLUSTER                 30 /*0x1e*/
#define BRN_PORT_NHOPCLUSTER              31 /*0x1f*/
/*Topology*/
#define BRN_PORT_TOPOLOGY_DETECTION       35 /*0x23*/
#define BRN_PORT_NHOPNEIGHBOURING         36 /*0x24*/
#define BRN_PORT_COMP_NHOPNEIGHBOURING    37 /*0x25*/
/*P2P*/
#define BRN_PORT_DHTROUTING               40 /*0x28*/
#define BRN_PORT_DHTSTORAGE               41 /*0x29*/
/*Data transfer*/
#define BRN_PORT_SDP                      50 /*0x32*/
#define BRN_PORT_TFTP                     51 /*0x33*/
#define BRN_PORT_FLOW                     52 /*0x34*/
#define BRN_PORT_COMPRESSION              53 /*0x35*/
/*Info*/
#define BRN_PORT_NODEINFO                 60 /*0x3c*/

#define DEFAULT_TTL 128
#define DEFAULT_TOS 0

#define BRN_HEADER_SIZE sizeof(click_brn)

class BRNProtocol : public Element { public:

  BRNProtocol();
  ~BRNProtocol();

  const char *class_name() const	{ return "BRNProtocol"; }

  static WritablePacket *add_brn_header(Packet *p, uint8_t dst_port, uint8_t src_port, uint8_t ttl=DEFAULT_TTL, uint8_t tos=DEFAULT_TOS);
  static int set_brn_header(Packet *p, uint8_t dst_port, uint8_t src_port, uint16_t len, uint8_t ttl=DEFAULT_TTL, uint8_t tos=DEFAULT_TOS);
  static void set_brn_header(uint8_t *data, uint8_t dst_port, uint8_t src_port, uint16_t len, uint8_t ttl=DEFAULT_TTL, uint8_t tos=DEFAULT_TOS);

  static struct click_brn* get_brnheader(Packet *p);
  static Packet *pull_brn_header(Packet *p);
  static WritablePacket *push_brn_header(Packet *p);

  static bool is_brn_etherframe(Packet *p);
  static struct click_brn* get_brnheader_in_etherframe(Packet *p);

};

CLICK_ENDDECLS
#endif
