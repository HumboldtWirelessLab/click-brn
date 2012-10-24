#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>

#include "elements/brn/brnprotocol/brnpacketanno.hh"

#include "brnprotocol.hh"

CLICK_DECLS

BRNProtocol::BRNProtocol()
{
}

BRNProtocol::~BRNProtocol()
{
}

WritablePacket *
BRNProtocol::add_brn_header(Packet *p, uint8_t dst_port, uint8_t src_port, uint8_t ttl, uint8_t tos) {
  WritablePacket *q;
  uint16_t size = p->length();

  if ( (q = push_brn_header(p)) != NULL )
    if ( set_brn_header(q, dst_port, src_port, size, ttl, tos) ) {
      BRNPacketAnno::set_ttl_anno(q, ttl); //TODO: Good idea to set annos here ??
      BRNPacketAnno::set_tos_anno(q, tos); //TODO: Good idea to set annos here ?? Maybe add bool to params,wether to do so

      return q;
    }

  return NULL;
}

int
BRNProtocol::set_brn_header(Packet *p, uint8_t dst_port, uint8_t src_port, uint16_t len, uint8_t ttl, uint8_t tos) {
  struct click_brn *brn_h;

  if ( ( brn_h = get_brnheader(p) ) != NULL ) {
    brn_h->dst_port = dst_port;
    brn_h->src_port = src_port;
    brn_h->body_length = htons(len);
    brn_h->ttl = ttl;
    brn_h->tos = tos;

    return 1;
  }

  return 0;
}

void
BRNProtocol::set_brn_header(uint8_t *data, uint8_t dst_port, uint8_t src_port, uint16_t len, uint8_t ttl, uint8_t tos) {
  struct click_brn *brn_h = (struct click_brn *)data;
  brn_h->dst_port = dst_port;
  brn_h->src_port = src_port;
  brn_h->body_length = htons(len);
  brn_h->ttl = ttl;
  brn_h->tos = tos;
}

struct click_brn*
BRNProtocol::get_brnheader(Packet *p) {
  if ( p->length() >= BRN_HEADER_SIZE )
    return (struct click_brn*)p->data();

  return NULL;
}

Packet *
BRNProtocol::pull_brn_header(Packet *p) {
  if ( p->length() >= BRN_HEADER_SIZE ) {
    p->pull(BRN_HEADER_SIZE);
    return p;
  }

  return NULL;
}

WritablePacket *
BRNProtocol::push_brn_header(Packet *p) {
  WritablePacket *q;
  if ( (q = p->push(BRN_HEADER_SIZE)) != NULL )
    return q;

  return NULL;
}

bool
BRNProtocol::is_brn_etherframe(Packet *p)
{
  return ((click_ether *)p->data())->ether_type == htons(ETHERTYPE_BRN);
}

struct click_brn*
BRNProtocol::get_brnheader_in_etherframe(Packet *p)
{
  return (struct click_brn*)&(p->data()[sizeof(click_ether)]);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNProtocol)

