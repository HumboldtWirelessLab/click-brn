#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "batmanprotocol.hh"
#include "elements/brn2/brn2.h"

CLICK_DECLS

BatmanProtocol::BatmanProtocol()
{
}

BatmanProtocol::~BatmanProtocol()
{
}

WritablePacket *
BatmanProtocol::add_batman_header(Packet *p, uint8_t type, uint8_t hops)
{
  WritablePacket *q;
  struct batman_header *bh;

  if ( (q = p->push(sizeof(struct batman_header))) != NULL ) {
    bh = (struct batman_header *)q->data();
    bh->type = type;
    bh->hops = hops;
    return q;
  }

  return NULL;
}

void
BatmanProtocol::rm_batman_header(Packet *p)
{
  p->pull(sizeof(struct batman_header));
}

struct batman_header *
BatmanProtocol::get_batman_header(Packet *p)
{
  return ((struct batman_header *)p->data());
}

/** include batman_header and batman_originator*/

WritablePacket *
BatmanProtocol::new_batman_originator( uint32_t id, uint8_t flag, EtherAddress *src, uint8_t neighbours )
{
  WritablePacket *p = WritablePacket::make( 96, NULL,
                                      sizeof(struct batman_header) + sizeof(struct batman_originator) + ( NEIGHBOURSIZE * neighbours ),
                                            32);

  struct batman_header *bh = (struct batman_header *)p->data();
  struct batman_originator *bo = (struct batman_originator *)&(p->data()[sizeof(struct batman_header)]);

  bh->type = BATMAN_ORIGINATOR;
  bh->hops = ORIGINATOR_SRC_HOPS;

  bo->id = htonl(id);
  bo->flag = flag;
  bo->neighbours = neighbours;
  memcpy(bo->src, src, 6);

  return p;
}

struct batman_originator *
BatmanProtocol::get_batman_originator(Packet *p)
{
  return ( (struct batman_originator *)&(p->data()[sizeof(struct batman_header)]));
}

uint8_t *
BatmanProtocol::get_batman_originator_payload(Packet *p)
{
  return ( (uint8_t *)&(p->data()[sizeof(struct batman_header) + sizeof(struct batman_originator)]));
}

WritablePacket *
BatmanProtocol::add_batman_routing(Packet *p, uint16_t flag, uint16_t id)
{
  WritablePacket *q;
  struct batman_routing *br;

  if ( (q = p->push(sizeof(struct batman_routing))) != NULL ) {
    br = (struct batman_routing *)q->data();
    br->flag = flag;
    br->id = htons(id);
    return q;
  }

  return NULL;
}

struct batman_routing *
BatmanProtocol::get_batman_routing(Packet *p)
{
  return ((struct batman_routing *)&(p->data()[sizeof(struct batman_header)]));
}

void
BatmanProtocol::rm_batman_routing(Packet *p)
{
  p->pull(sizeof(struct batman_routing));
}

void
BatmanProtocol::rm_batman_routing_header(Packet *p)
{
  p->pull(sizeof(struct batman_header) + sizeof(struct batman_routing));
}


struct click_ether *
BatmanProtocol::get_ether_header(Packet *p)
{
  return ( (struct click_ether *)&(p->data()[sizeof(struct batman_header) + sizeof(struct batman_routing)]));
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BatmanProtocol)

