#ifndef HAWKPROTOCOL_HH
#define HAWKPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/dht/standard/dhtnode.hh"

CLICK_DECLS


struct hawk_routing_header {
  uint8_t _next_etheraddress[6];              //TODO: needed ??
  uint8_t _next_nodeid[MAX_NODEID_LENTGH];    //TODO: needed ??
  int _next_nodeid_length;                    //TODO: needed ??

  uint8_t _dst_nodeid[MAX_NODEID_LENTGH];
  int _dst_nodeid_length;                     //TODO: needed ??

  uint8_t _src_nodeid[MAX_NODEID_LENTGH];
  int _src_nodeid_length;                     //TODO: needed ??
} CLICK_SIZE_PACKED_ATTRIBUTE;

class HawkProtocol {

 public:

  HawkProtocol();
  ~HawkProtocol();

  static WritablePacket *add_route_header(uint8_t *dst_nodeid, int dst_nodeid_length,
                                          uint8_t *src_nodeid, int src_nodeid_length,
                                          Packet *p);
  static struct hawk_routing_header *get_route_header(Packet *p);
  static click_ether *get_ether_header(Packet *p);
  static void strip_route_header(Packet *p);
};

CLICK_ENDDECLS
#endif
