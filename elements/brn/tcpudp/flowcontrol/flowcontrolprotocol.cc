#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "flowcontrolprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
CLICK_DECLS


WritablePacket *
FlowControlProtocol::add_header(Packet *p, uint16_t type, uint16_t flowid, uint16_t seq)
{
  struct flowcontrol_header *header;
  WritablePacket *flc_p = p->push(sizeof(struct flowcontrol_header));

  header = (struct flowcontrol_header *)flc_p->data();
  header->_flags = htons(type);
  header->_flow_id = htons(flowid);
  header->_seq_number = htons(seq);

  return(flc_p);
}

struct flowcontrol_header *
FlowControlProtocol::get_header(Packet *p)
{
  return (struct flowcontrol_header *)p->data();
}

void
FlowControlProtocol::get_info(Packet *p, uint16_t *type, uint16_t *flowid, uint16_t *seq)
{
  struct flowcontrol_header *header = (struct flowcontrol_header *)p->data();
  *type = ntohs(header->_flags);
  *flowid = ntohs(header->_flow_id);
  *seq = ntohs(header->_seq_number);
}

void
FlowControlProtocol::strip_header(Packet *p)
{
  p->pull(sizeof(struct flowcontrol_header));
}

WritablePacket *
FlowControlProtocol::make_ack(uint16_t flowid, uint16_t seq)
{
  WritablePacket *new_packet = WritablePacket::make( 256, NULL,
                                                     sizeof(struct flowcontrol_header), 32);

  struct flowcontrol_header *header = (struct flowcontrol_header *)new_packet->data();
  header->_flags = htons(FC_TYPE_ACK);
  header->_flow_id = htons(flowid);
  header->_seq_number = htons(seq);

  return new_packet;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(FlowControlProtocol)

