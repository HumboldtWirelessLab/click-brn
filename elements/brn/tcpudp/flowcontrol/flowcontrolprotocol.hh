#ifndef FLOWCONTROLPROTOCOL_HH
#define FLOWCONTROLPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"

CLICK_DECLS

#define FC_TYPE_DATA        0
#define FC_TYPE_ACK         1
#define FC_TYPE_RTT_REQUEST 2
#define FC_TYPE_RTT_REPLY   3

struct flowcontrol_header {
  //TODO: use 16 bits for flags and flowid

  uint16_t _flags;

  uint16_t _flow_id;

  uint16_t _seq_number;

} CLICK_SIZE_PACKED_ATTRIBUTE;



class FlowControlProtocol {

 public:

  FlowControlProtocol();
  ~FlowControlProtocol();

  static WritablePacket *add_header(Packet *p, uint16_t type, uint16_t flowid, uint16_t seq);

  static struct flowcontrol_header *get_header(Packet *p);
  static void get_info(Packet *p, uint16_t *type, uint16_t *flowid, uint16_t *seq);
  static void strip_header(Packet *p);

  static WritablePacket *make_ack(uint16_t flowid, uint16_t seq);

};

CLICK_ENDDECLS
#endif
