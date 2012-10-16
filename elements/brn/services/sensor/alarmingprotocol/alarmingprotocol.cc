#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "alarmingprotocol.hh"

CLICK_DECLS

AlarmingProtocol::AlarmingProtocol()
{
}

AlarmingProtocol::~AlarmingProtocol()
{
}

WritablePacket *
AlarmingProtocol::new_alarming_packet(int type)
{
  WritablePacket *p = WritablePacket::make(128 /*headroom*/,NULL /* *data*/, sizeof(struct alarming_header), 32);
  struct alarming_header *ah = (struct alarming_header *)p->data();
  ah->type = type;

  return p;
}

int
AlarmingProtocol::get_count_nodes(Packet *p)
{
  return (p->length() - sizeof(struct alarming_header)) / sizeof(struct alarming_node);
}

int
AlarmingProtocol::get_node(Packet *p, uint8_t node_index, EtherAddress *ea, uint8_t *ttl, uint8_t *id)
{
  struct alarming_node *an = (struct alarming_node *)&(p->data()[sizeof(struct alarming_header)]);
  *ea = EtherAddress(an[node_index].node_ea);
  *ttl = an[node_index].ttl;
  *id = an[node_index].id;
  return 0;
}

struct alarming_node*
AlarmingProtocol::get_node_struct(Packet *p, uint8_t node_index)
{
  struct alarming_node *an = (struct alarming_node *)&(p->data()[sizeof(struct alarming_header)]);
  return &(an[node_index]);
}

struct alarming_node*
AlarmingProtocol::get_node_struct(Packet *p, EtherAddress *ea, uint8_t id)
{
  uint32_t index = sizeof(struct alarming_header);

  while ( index < p->length() ) {
    struct alarming_node *an = (struct alarming_node *)&(p->data()[index]);
    if ((id == an->id) && (memcmp(ea->data(), an->node_ea,6) == 0)) return an;
    index += sizeof(struct alarming_node);
  }
  return NULL;
}

WritablePacket *
AlarmingProtocol::add_node(Packet *p, const EtherAddress *node, int ttl, int id) {
  WritablePacket *p_new = p->put(sizeof(struct alarming_node));
  struct alarming_node *an = (struct alarming_node *)&(p_new->data()[p_new->length() - sizeof(struct alarming_node)]);

  memcpy(an->node_ea,node->data(),sizeof(an->node_ea));
  an->ttl = ttl;
  an->id = id;

  return p_new;
}

int
AlarmingProtocol::remove_node_with_high_ttl(Packet *p, uint8_t ttl)
{
  struct alarming_node *an = (struct alarming_node *)&(p->data()[sizeof(struct alarming_header)]);
  int no_nodes = (p->length() - sizeof(struct alarming_header)) / sizeof(struct alarming_node);

  int removed_nodes = 0;
  for ( int i = no_nodes - 1; i >= 0; i-- ) {
    if ( an[i].ttl > ttl ) { //remove node
      if ( (no_nodes - removed_nodes - 1) > i ) {
        /*move old nodes*/
        memcpy(&(an[i]),&(an[i+1]),(no_nodes - removed_nodes - i));
      }
      p->take(sizeof(struct alarming_node));
      removed_nodes++;
    }
  }

  return (no_nodes - removed_nodes);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AlarmingProtocol)
