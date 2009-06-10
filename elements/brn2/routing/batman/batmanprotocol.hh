#ifndef CLICK_BATMANPROTOCOL_HH
#define CLICK_BATMANPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

CLICK_DECLS

struct batman_header {
  uint8_t type;
  uint8_t hops;
} __attribute__ ((packed));

struct batman_originator {
  uint8_t  flag;
  uint8_t  neighbours;
  uint32_t id;
  uint8_t  src[6];
} __attribute__ ((packed));

/** size of a single neighbour is etheraddress-size*/
#define NEIGHBOURSIZE 6

struct batman_routing {
  uint16_t flag;
  uint16_t id;
} __attribute__ ((packed));

/********************** Originator *************************
| WIFIHEADER | BRNHEADER | BATMANHEADER | BATMANORIGINATOR |
***********************************************************/

/************************ Routing ****************************************
| WIFIHEADER | BRNHEADER | BATMANHEADER | BATMANROUTING | ETHERNETFRAME |
*************************************************************************/

#define BATMAN_UNKNOWN         0
#define BATMAN_ORIGINATOR      1
#define BATMAN_ROUTING_FORWARD 2
#define BATMAN_ROUTING_ERROR   3

/**
 TODO: check type and header while getting wanted header
*/


class BatmanProtocol : public Element { public:

  BatmanProtocol();
  ~BatmanProtocol();

  const char *class_name() const	{ return "BatmanProtocol"; }

  static WritablePacket *add_batman_header(Packet *p, uint8_t type, uint8_t hops);
  static struct batman_header *get_batman_header(Packet *p);

  static WritablePacket *new_batman_originator( uint32_t id, uint8_t flag, EtherAddress *src, uint8_t neighbours = 0);
  static struct batman_originator *get_batman_originator(Packet *p);
  static uint8_t *get_batman_originator_payload(Packet *p);

  static WritablePacket *add_batman_routing(Packet *p, uint16_t flag, uint16_t id);
  static struct batman_routing *get_batman_routing(Packet *p);

  static struct click_ether *get_ether_header(Packet *p);
};

CLICK_ENDDECLS
#endif
