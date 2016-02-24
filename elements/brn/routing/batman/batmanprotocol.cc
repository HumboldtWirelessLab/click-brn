#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "batmanprotocol.hh"
#include "elements/brn/brn2.h"

CLICK_DECLS

/*********************************************************************************************/
/************************************ G E N E R A L ******************************************/
/*********************************************************************************************/

WritablePacket *
BatmanProtocol::add_batman_header(Packet *p, uint8_t type, uint8_t hops)
{
  WritablePacket *q;

  if ( (q = p->push(sizeof(struct batman_header))) != NULL ) {
    struct batman_header *bh = (struct batman_header *)q->data();
    bh->flags = 0;
    bh->type  = type;
    bh->hops  = hops;
    return q;
  }

  return NULL;
}

void
BatmanProtocol::pull_batman_header(Packet *p)
{
  p->pull(sizeof(struct batman_header));
}

struct batman_header *
BatmanProtocol::get_batman_header(Packet *p)
{
  return ((struct batman_header *)p->data());
}

/*********************************************************************************************/
/******************************* O R I G I N A T O R *****************************************/
/*********************************************************************************************/

/** include batman_header and batman_originator*/

WritablePacket *
BatmanProtocol::new_batman_originator( uint32_t id, uint8_t flags, uint8_t neighbours )
{
  WritablePacket *p = WritablePacket::make( 96, NULL,
                                              sizeof(struct batman_header) +
                                              sizeof(struct batman_originator) + ( NEIGHBOURSIZE * neighbours ), 32);

  struct batman_header *bh = (struct batman_header *)p->data();
  struct batman_originator *bo = (struct batman_originator *)&(p->data()[sizeof(struct batman_header)]);

  bh->type = BATMAN_ORIGINATOR;
  bh->hops = 0;
  bh->flags = flags;

  bo->id = htonl(id);

  return p;
}

struct batman_originator *
BatmanProtocol::get_batman_originator(Packet *p)
{
  return ( (struct batman_originator *)&(p->data()[sizeof(struct batman_header)]));
}

uint32_t
BatmanProtocol::get_batman_originator_id(Packet *p)
{
  return ntohl(((struct batman_originator *)&(p->data()[sizeof(struct batman_header)]))->id);
}

uint8_t *
BatmanProtocol::get_batman_originator_payload(Packet *p)
{
  return ( (uint8_t *)&(p->data()[sizeof(struct batman_header) + sizeof(struct batman_originator)]));
}

void
BatmanProtocol::add_batman_node(Packet *p, struct batman_node *new_bn, uint32_t index)
{
  struct batman_node *bn = get_batman_node(p, index);
  memcpy(bn, new_bn, sizeof(struct batman_node));
}

uint32_t
BatmanProtocol::count_batman_nodes(Packet *p)
{
  assert(p->length() >= (sizeof(struct batman_header)+sizeof(struct batman_originator)));
  return ((p->length()-(sizeof(struct batman_header)+sizeof(struct batman_originator)))/sizeof(struct batman_node));
}

struct batman_node *
BatmanProtocol::get_batman_node(Packet *p, uint32_t index)
{
  assert(count_batman_nodes(p) > index);

  return &(((struct batman_node*)&(p->data()[sizeof(struct batman_header)+sizeof(struct batman_originator)]))[index]);
}

/*********************************************************************************************/
/************************************ R O U T I N G ******************************************/
/*********************************************************************************************/

WritablePacket *
    BatmanProtocol::add_batman_routing(Packet *p, uint16_t flag, uint16_t id)
{
  WritablePacket *q;

  if ( (q = p->push(sizeof(struct batman_routing))) != NULL ) {
    struct batman_routing *br = (struct batman_routing *)q->data();
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
BatmanProtocol::pull_batman_routing(Packet *p)
{
  p->pull(sizeof(struct batman_routing));
}

void
BatmanProtocol::pull_batman_routing_header(Packet *p)
{
  p->pull(sizeof(struct batman_header) + sizeof(struct batman_routing));
}


struct click_ether *
BatmanProtocol::get_ether_header(Packet *p)
{
  return ( (struct click_ether *)&(p->data()[sizeof(struct batman_header) + sizeof(struct batman_routing)]));
}

/*********************************************************************************************/
/************************************ R O U T I N G ******************************************/
/*********************************************************************************************/

WritablePacket *
BatmanProtocol::add_batman_error(Packet *p, uint8_t code, EtherAddress *src)
{
  WritablePacket *q;

  if ( (q = p->push(sizeof(struct batman_routing_error))) != NULL ) {
    struct batman_routing_error *bre = (struct batman_routing_error *)q->data();
    bre->error_code = code;
    bre->packet_info = BATMAN_ERROR_PACKET_FULL;
    memcpy(bre->error_src, src->data(), 6);
    return q;
  }

  return NULL;
}

struct batman_routing_error *
BatmanProtocol::get_batman_error(Packet *p)
{
  return ( (struct batman_routing_error *)&(p->data()[sizeof(struct batman_header)]));
}

void
BatmanProtocol::pull_batman_error(Packet *p)
{
  p->pull(sizeof(struct batman_header) + sizeof(struct batman_routing) + sizeof(struct batman_routing_error));
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(BatmanProtocol)

