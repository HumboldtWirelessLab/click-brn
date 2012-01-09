#ifndef CLICK_BATMANPROTOCOL_HH
#define CLICK_BATMANPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

#define BATMAN_MAX_ORIGINATOR_HOPS 50

struct batman_header {
#if BYTE_ORDER == BIG_ENDIAN
  uint8_t type:4;
  uint8_t flags:4;
#else
  uint8_t flags:4;
  uint8_t type:4;
#endif
  uint8_t hops;
} __attribute__ ((packed));

struct batman_originator {
  uint32_t id;
} __attribute__ ((packed));

struct batman_node {
  uint32_t id;
  uint16_t metric;
  uint8_t flags;
  uint8_t hops;
  uint8_t src[6];
} __attribute__ ((packed));


/** size of a single neighbour is etheraddress-size*/
#define NEIGHBOURSIZE sizeof(struct batman_node)

struct batman_routing {
  uint16_t flag;
  uint16_t id;
} __attribute__ ((packed));

struct batman_routing_error {
  uint8_t error_code;
  uint8_t packet_info;
  uint8_t error_src[6];
} __attribute__ ((packed));

/********************** Originator *****************************************************
| WIFIHEADER | BRNHEADER | BATMANHEADER | BATMANORIGINATOR | BATMANNODES | BATMANNO... |
***************************************************************************************/

/************************ Routing ***************************************
| WIFIHEADER | BRNHEADER | BATMANHEADER | BATMANROUTING | ETHERNETFRAME |
*************************************************************************/

/************************ Routing Error ******************************************************
| WIFIHEADER | BRNHEADER | BATMANHEADER | BATMANROUTINGERROR | BATMANROUTING | ETHERNETFRAME |
*********************************************************************************************/

/***** TYPE ******/
#define BATMAN_UNKNOWN         0
#define BATMAN_ORIGINATOR      1
#define BATMAN_ROUTING_FORWARD 2
#define BATMAN_ROUTING_ERROR   3

/******* ORIGINATOR FLAGS *******/
#define BATMAN_ORIGINATOR_FWD_MODE            1
#define BATMAN_ORIGINATOR_FWD_MODE_SINGLE_HOP 1
#define BATMAN_ORIGINATOR_FWD_MODE_FULL_PATH  0

/*********** ERROR CODES ********/
#define BATMAN_ERROR_CODE_TTL_EXPIRED      1 << 0
#define BATMAN_ERROR_CODE_UNKNOWN_DST      1 << 1
#define BATMAN_ERROR_CODE_LOOP_DETECTED    1 << 2

/*********** PACKET INFO ********/
#define BATMAN_ERROR_PACKET_MODE            3
#define BATMAN_ERROR_PACKET_FULL            0
#define BATMAN_ERROR_PACKET_HEADER          1
#define BATMAN_ERROR_PACKET_NONE            2

/**
 TODO: check type and header while getting wanted header
*/


class BatmanProtocol : public Element { public:

  BatmanProtocol() {};
  ~BatmanProtocol() {};

  const char *class_name() const	{ return "BatmanProtocol"; }

  /* General */
  static WritablePacket *add_batman_header(Packet *p, uint8_t type, uint8_t hops);
  static struct batman_header *get_batman_header(Packet *p);
  static void pull_batman_header(Packet *p);

  /* Originator */
  static WritablePacket *new_batman_originator( uint32_t id, uint8_t flag, uint8_t nodes = 0);
  static struct batman_originator *get_batman_originator(Packet *p);
  static uint32_t get_batman_originator_id(Packet *p);
  static uint8_t *get_batman_originator_payload(Packet *p);
  static void add_batman_node(Packet *p, struct batman_node *new_bn, uint32_t index);
  static uint32_t count_batman_nodes(Packet *p);
  static struct batman_node *get_batman_node(Packet *p, uint32_t index);

  static uint8_t get_originator_fwd_mode(Packet *p);

  /* Routing */
  static WritablePacket *add_batman_routing(Packet *p, uint16_t flag, uint16_t id);
  static struct batman_routing *get_batman_routing(Packet *p);
  static void pull_batman_routing(Packet *p);
  static void pull_batman_routing_header(Packet *p);

  static struct click_ether *get_ether_header(Packet *p);

  /* Error */
  static WritablePacket *add_batman_error(Packet *p, uint8_t code, EtherAddress *src);
  static struct batman_routing_error *get_batman_error(Packet *p);
  static void pull_batman_error(Packet *p);

};

CLICK_ENDDECLS
#endif
