#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "nhopclusterprotocol.hh"

CLICK_DECLS

int
NHopClusterProtocol::get_type(Packet *p, int offset) {
  struct nhopcluster_packet_header *h;
  h = (struct nhopcluster_packet_header *)&(p->data()[offset]);
  return h->packet_type;
}

int
NHopClusterProtocol::get_operation(Packet *p, int offset) {
  struct nhopcluster_managment *m;
  m = (struct nhopcluster_managment *)&(p->data()[offset + sizeof(struct nhopcluster_packet_header)]);
  return m->operation;
}

struct nhopcluster_managment *
NHopClusterProtocol::get_mgt(Packet *p, int offset)
{
  if ( (p->length() - offset) >= (sizeof(struct nhopcluster_managment) + sizeof(struct nhopcluster_packet_header)))
    return ((struct nhopcluster_managment *)&(p->data()[offset + sizeof(struct nhopcluster_packet_header)]));

  return NULL;
}

WritablePacket*
NHopClusterProtocol::new_request(int max_hops, int code, int id)
{
  WritablePacket *p = WritablePacket::make(128 /*headroom*/,NULL /* *data*/,
                           sizeof(struct nhopcluster_managment) + sizeof(struct nhopcluster_packet_header),
                           32);

  if ( p != NULL ) {
    struct nhopcluster_packet_header *h = (struct nhopcluster_packet_header *)p->data();
    struct nhopcluster_managment *m = (struct nhopcluster_managment *)&(p->data()[sizeof(struct nhopcluster_packet_header)]);

    h->packet_type = NHOPCLUSTER_PACKETTYPE_MANAGMENT;
    m->operation = NHOPCLUSTER_MANAGMENT_REQUEST;
    m->hops = max_hops;
    m->code = code;

    m->id = id;
    memset(m->clusterhead,0,6);
  }

  return p;
}

WritablePacket*
NHopClusterProtocol::new_notify(EtherAddress *ch, int max_hops, int code, int id)
{
  WritablePacket *p = WritablePacket::make(128 /*headroom*/,NULL /* *data*/,
                           sizeof(struct nhopcluster_managment) + sizeof(struct nhopcluster_packet_header),
                                  32);

  if ( p != NULL ) {
    struct nhopcluster_packet_header *h = (struct nhopcluster_packet_header *)p->data();
    struct nhopcluster_managment *m = (struct nhopcluster_managment *)&(p->data()[sizeof(struct nhopcluster_packet_header)]);

    h->packet_type = NHOPCLUSTER_PACKETTYPE_MANAGMENT;
    m->operation = NHOPCLUSTER_MANAGMENT_NOTIFICATION;
    m->hops = max_hops;
    m->code = code;

    m->id = htonl(id);
    memcpy(m->clusterhead,ch->data(),6);
  }

  return p;
}

int
NHopClusterProtocol::pack_lp(struct nhopcluster_lp_info *lpi, uint8_t *data,  int max_size)
{
  if ( (uint32_t)max_size >= ( sizeof(struct nhopcluster_lp_info) + sizeof(struct nhopcluster_packet_header) )) {
    struct nhopcluster_packet_header *h = (struct nhopcluster_packet_header *)data;
    h->packet_type = NHOPCLUSTER_PACKETTYPE_LP_INFO;
    memcpy(&data[sizeof(struct nhopcluster_packet_header)], (int8_t*)lpi, sizeof(struct nhopcluster_lp_info));
    //click_chatter("Maxsize. %d Size; %d",max_size,sizeof(struct nhopcluster_lp_info) + sizeof(struct nhopcluster_packet_header));
    return (sizeof(struct nhopcluster_lp_info) + sizeof(struct nhopcluster_packet_header));
  }

  //click_chatter("Unable to pack");

  return 0;
}

int
NHopClusterProtocol::unpack_lp(struct nhopcluster_lp_info *lpi, uint8_t *data, int size)
{
  if ( size == ( sizeof(struct nhopcluster_lp_info) + sizeof(struct nhopcluster_packet_header) )) {
    struct nhopcluster_packet_header *h = (struct nhopcluster_packet_header *)data;
    if ( h->packet_type != NHOPCLUSTER_PACKETTYPE_LP_INFO ) return 0;

    memcpy((int8_t*)lpi, &data[sizeof(struct nhopcluster_packet_header)], sizeof(struct nhopcluster_lp_info));
  }

  return size;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(NHopClusterProtocol)

