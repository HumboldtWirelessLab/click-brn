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

#define HAWK_FLAGS_SOURCE_ROUTE   1
#define HAWK_FLAGS_HAS_ROUTE_INFO 2

/*****************************************/
/************* General header ************/
/*****************************************/

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

/********************************************/
/*** Header and structs for Source routes ***/
/********************************************/

struct hawk_routing_source_route {
  uint8_t hops;
  union {
    uint8_t next_hop;
    uint8_t flags;
  };
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct hawk_routing_source_route_hop {
  uint8_t address[6];
} CLICK_SIZE_PACKED_ATTRIBUTE;

/***********************************************/
/*** Header and structs for annotated routes ***/
/***********************************************/

struct hawk_extra_routing_info {
  uint16_t offset;
} CLICK_SIZE_PACKED_ATTRIBUTE;

/***********************************************/
/***  PACKET Layout                          ***/
/***********************************************/

/*
  -------------------------------------
 |       hawk_routing_header          |
 --------------------------------------
 |    hawk_routing_source_route       |  Optional
 --------------------------------------
 |  hawk_routing_source_route_hop     |  Optional: N-times
 --------------------------------------
 |       hawk_extra_routing_info      |
 --------------------------------------
 |          DATA (Payload)            |
 --------------------------------------
 |    hawk_routing_source_route       |  Optional: annotated route
 --------------------------------------
 |  hawk_routing_source_route_hop     |  Optional: N-times
 --------------------------------------

*/
/***********************************************/
/***                                         ***/
/***********************************************/

class HawkProtocol {

 public:

  HawkProtocol();
  ~HawkProtocol();

  static WritablePacket *add_route_header(uint8_t *dst_nodeid, uint8_t *src_nodeid,
                                          Packet *p);

  static WritablePacket *add_route_header(uint8_t *dst_nodeid, uint8_t *src_nodeid,
                                          uint8_t *next_nodeid, EtherAddress *_next,
                                          Packet *p);

  static WritablePacket *add_route_header(uint8_t *dst_nodeid, uint8_t *src_nodeid,
                                          uint8_t *next_nodeid, EtherAddress *_next,
                                          struct hawk_routing_source_route_hop *route, //TODO: use Vector like Routingtable
                                          uint16_t route_len,
                                          Packet *p);

  static struct hawk_routing_header *get_route_header(Packet *p, Vector<EtherAddress> *route);

  static click_ether *get_ether_header(Packet *p);
  static void strip_route_header(Packet *p);
  static void add_metric(Packet *p,uint8_t metric);
  static void set_rew_metric(Packet *p,uint8_t metric);
  static void set_next_hop(Packet *p, EtherAddress *next,uint8_t* next_nodeid);
  static bool has_next_hop(Packet *p);
  static void clear_next_hop(Packet *p);

  static WritablePacket *add_route_info(struct hawk_routing_source_route_hop *route, //TODO: use Vector like Routingtable
                                        uint16_t route_len,
                                        Packet *p);

  static int get_route_info(struct hawk_routing_source_route_hop *route, //TODO: use Vector like Routingtable
                            uint16_t max_len,
                            Packet *p);

};

CLICK_ENDDECLS
#endif
