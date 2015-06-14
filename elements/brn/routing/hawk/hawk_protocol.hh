#ifndef HAWKPROTOCOL_HH
#define HAWKPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/dht/standard/dhtnode.hh"

CLICK_DECLS

struct hawk_routing_header {
#define HAWK_FLAGS_ROUTE_FIXED 1

  uint8_t _flags;

  uint8_t _metric;

  uint8_t _rew_metric;

  uint8_t _next_etheraddress[6];

  uint8_t _next_nodeid[MAX_NODEID_LENTGH];

  uint8_t _dst_nodeid[MAX_NODEID_LENTGH];

  uint8_t _src_nodeid[MAX_NODEID_LENTGH];

} CLICK_SIZE_PACKED_ATTRIBUTE;

class HawkProtocol {

 public:

  HawkProtocol();
  ~HawkProtocol();

  static WritablePacket *add_route_header(uint8_t *dst_nodeid, uint8_t *src_nodeid,
                                          Packet *p);

  static WritablePacket *add_route_header(uint8_t *dst_nodeid, uint8_t *src_nodeid,
                                          uint8_t *next_nodeid, EtherAddress *_next,
                                          Packet *p);

  static struct hawk_routing_header *get_route_header(Packet *p);
  static click_ether *get_ether_header(Packet *p);
  static void strip_route_header(Packet *p);
  static void add_metric(Packet *p,uint8_t metric);
  static void set_rew_metric(Packet *p,uint8_t metric);
  static void set_next_hop(Packet *p, EtherAddress *next,uint8_t* next_nodeid);
  static bool has_next_hop(Packet *p);
  static void clear_next_hop(Packet *p);
};

CLICK_ENDDECLS
#endif
