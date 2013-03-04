#ifndef COMPRESSEDALARMINGPROTOCOL_HH
#define COMPRESSEDALARMINGPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

#define DEFAULT_HOP_INVALID 255
#define DEFAULT_HOP_LIMT 5

#define START_TTL 150

#define DEFAULT_RETRY_LIMIT 3

#define DEFAULT_MIN_NEIGHBOUT_FRACTION 95

struct compressed_alarming_header {
  uint8_t type;
} __attribute__ ((packed));

struct compressed_alarming_node {
  uint8_t node_ea[6];
  uint8_t ttl;
  uint8_t id;
} __attribute__ ((packed));

class CompressedAlarmingProtocol {

 public:

  static WritablePacket *new_alarming_packet(int type);

  static struct compressed_alarming_header *getCompressedAlarmingHeader(Packet *p) { return ((struct compressed_alarming_header *)p->data()); }
  static int get_count_nodes(Packet *p);

  static int get_node(Packet *p, uint8_t node_index, EtherAddress *ea, uint8_t *ttl, uint8_t *id);
  static struct compressed_alarming_node *get_node_struct(Packet *p, uint8_t node_index);
  static struct compressed_alarming_node *get_node_struct(Packet *p, EtherAddress *ea, uint8_t id);

  static WritablePacket *add_node(Packet *p, const EtherAddress *node, int ttl, int id);
  static int remove_node_with_high_ttl(Packet *p, uint8_t ttl);

};

CLICK_ENDDECLS
#endif
