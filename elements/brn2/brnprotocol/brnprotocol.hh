#ifndef CLICK_BRNPROTOCOL_HH
#define CLICK_BRNPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS
/*
CLICK_SIZE_PACKED_STRUCTURE(
struct click_brn {,
  uint8_t   dst_port;
  uint8_t   src_port;
  uint16_t  body_length;
  uint8_t   ttl;
  uint8_t   tos;
});

typedef struct click_brn brn_header;
*/

#define ETHERTYPE_BRN          0x8086 /* Berlin Roofnet Protocol */
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

#define BRN_TOS_BE     0
#define BRN_TOS_HP     1

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

#define BRN_PORT_SDP              1
#define BRN_PORT_TFTP             2
#define BRN_PORT_DSR              3
#define BRN_PORT_BCAST            4
#define BRN_PORT_LINK_PROBE       6
#define BRN_PORT_DHT              7
#define BRN_PORT_IAPP             8
#define BRN_PORT_GATEWAY          9
#define BRN_PORT_FLOW            16


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

};

CLICK_ENDDECLS
#endif
