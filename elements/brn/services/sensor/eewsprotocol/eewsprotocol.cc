#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "eewsprotocol.hh"

CLICK_DECLS

EewsProtocol::EewsProtocol()
{
}

EewsProtocol::~EewsProtocol()
{
}

WritablePacket *
EewsProtocol::new_eews_packet(int type)
{
  WritablePacket *p = WritablePacket::make(128 /*headroom*/,NULL /* *data*/, sizeof(struct eews_header), 32);
  struct eews_header *ah = (struct eews_header *)p->data();
  ah->type = type;
  //ah->x =

  return p;
}

int
EewsProtocol::get_count_nodes(Packet *p)
{
  return (p->length() - sizeof(struct eews_header)) / sizeof(struct eews_node);
}

int
EewsProtocol::get_node(Packet *p, uint8_t node_index, EtherAddress *ea, uint8_t *ttl, uint8_t *id)
{
  struct eews_node *an = (struct eews_node *)&(p->data()[sizeof(struct eews_header)]);
  *ea = EtherAddress(an[node_index].node_ea);
  *ttl = an[node_index].ttl;
  *id = an[node_index].id;
  return 0;
}

struct eews_node*
EewsProtocol::get_node_struct(Packet *p, uint8_t node_index)
{
  struct eews_node *an = (struct eews_node *)&(p->data()[sizeof(struct eews_header)]);
  return &(an[node_index]);
}

struct eews_node*
EewsProtocol::get_node_struct(Packet *p, EtherAddress *ea, uint8_t id)
{
  uint32_t index = sizeof(struct eews_header);

  while ( index < p->length() ) {
    struct eews_node *an = (struct eews_node *)&(p->data()[index]);
    if ((id == an->id) && (memcmp(ea->data(), an->node_ea,6) == 0)) return an;
    index += sizeof(struct eews_node);
  }
  return NULL;
}

WritablePacket *
EewsProtocol::add_node(Packet *p, const EtherAddress *node, int ttl, int id) {
  WritablePacket *p_new = p->put(sizeof(struct eews_node));
  struct eews_node *an = (struct eews_node *)&(p_new->data()[p_new->length() - sizeof(struct eews_node)]);

  memcpy(an->node_ea,node->data(),sizeof(an->node_ea));
  an->ttl = ttl;
  an->id = id;

  return p_new;
}

int
EewsProtocol::remove_node_with_high_ttl(Packet *p, uint8_t ttl)
{
  struct eews_node *an = (struct eews_node *)&(p->data()[sizeof(struct eews_header)]);
  int no_nodes = (p->length() - sizeof(struct eews_header)) / sizeof(struct eews_node);

  int removed_nodes = 0;
  for ( int i = no_nodes - 1; i >= 0; i-- ) {
    if ( an[i].ttl > ttl ) { //remove node
      if ( (no_nodes - removed_nodes - 1) > i ) {
        /*move old nodes*/
        memcpy(&(an[i]),&(an[i+1]),(no_nodes - removed_nodes - i));
      }
      p->take(sizeof(struct eews_node));
      removed_nodes++;
    }
  }

  return (no_nodes - removed_nodes);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EewsProtocol)
