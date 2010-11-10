#ifndef ALARMINGPROTOCOL_HH
#define ALARMINGPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

struct alarming_header {
  uint8_t type;
} __attribute__ ((packed));

struct alarming_node {
  uint8_t node_ea[6];
  uint8_t ttl;
  uint8_t id;
} __attribute__ ((packed));

class AlarmingProtocol : public Element {

 public:

  AlarmingProtocol();
  ~AlarmingProtocol();

  const char *class_name() const { return "AlarmingProtocol"; }

  static WritablePacket *new_alarming_packet(int type);

  static struct alarming_header *getAlarmingHeader(Packet *p) { return ((struct alarming_header *)p->data()); }
  static int get_count_nodes(Packet *p);

  static int get_node(Packet *p, uint8_t node_index, EtherAddress *ea, uint8_t *ttl, uint8_t *id);
  static struct alarming_node *get_node_struct(Packet *p, uint8_t node_index);
  static struct alarming_node *get_node_struct(Packet *p, EtherAddress *ea, uint8_t id);

  static WritablePacket *add_node(Packet *p, const EtherAddress *node, int ttl, int id);
  static int remove_node_with_high_ttl(Packet *p, uint8_t ttl);

};

CLICK_ENDDECLS
#endif
