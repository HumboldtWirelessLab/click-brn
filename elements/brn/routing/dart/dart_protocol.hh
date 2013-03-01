#ifndef DARTPROTOCOL_HH
#define DARTPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/dht/standard/dhtnode.hh"

CLICK_DECLS

struct dht_nodeid_entry {
  uint16_t _id_length;
  uint8_t _nodeid[MAX_NODEID_LENTGH];
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct dart_routing_header {
  uint16_t _dst_nodeid_length;
  uint8_t _dst_nodeid[MAX_NODEID_LENTGH];

  uint16_t _src_nodeid_length;
  uint8_t _src_nodeid[MAX_NODEID_LENTGH];
} CLICK_SIZE_PACKED_ATTRIBUTE;


class DartProtocol {

 public:

  DartProtocol();
  ~DartProtocol();

  static WritablePacket *add_route_header(uint8_t *dst_nodeid, int dst_nodeid_length, uint8_t *src_nodeid, int src_nodeid_length, Packet *p);
  static struct dart_routing_header *get_route_header(Packet *p);
  static click_ether *get_ether_header(Packet *p);
  static void strip_route_header(Packet *p);
};

CLICK_ENDDECLS
#endif
