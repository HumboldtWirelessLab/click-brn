#ifndef CLICK_BRNDHCPPROTOCOL_HH
#define CLICK_BRNDHCPPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "elements/brn/brn.h"

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

#define DEFAULT_TTL 128
#define DEFAULT_TOS 0

#define BRN_HEADER_SIZE sizeof(click_brn)

class DHCPProtocol : public Element { public:

  DHCPProtocol();
  ~DHCPProtocol();

  const char *class_name() const	{ return "DHCPProtocol"; }

  static WritablePacket *add_brn_header(Packet *p, uint8_t dst_port, uint8_t src_port, uint8_t ttl=DEFAULT_TTL, uint8_t tos=DEFAULT_TOS);
  static int set_brn_header(Packet *p, uint8_t dst_port, uint8_t src_port, uint16_t len, uint8_t ttl=DEFAULT_TTL, uint8_t tos=DEFAULT_TOS);
  static void set_brn_header_data(uint8_t *data, uint8_t dst_port, uint8_t src_port, uint16_t len, uint8_t ttl=DEFAULT_TTL, uint8_t tos=DEFAULT_TOS);

  static struct click_brn* get_brnheader(Packet *p);
  static Packet *pull_brn_header(Packet *p);
  static WritablePacket *push_brn_header(Packet *p);

};

CLICK_ENDDECLS
#endif
