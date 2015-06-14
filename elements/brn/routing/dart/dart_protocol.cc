#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "dart_protocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
CLICK_DECLS


WritablePacket *
DartProtocol::add_route_header(uint8_t *dst_nodeid, uint16_t dst_nodeid_length, uint8_t *src_nodeid, uint16_t src_nodeid_length,uint8_t* ident , Packet *p)
{
  struct dart_routing_header *header;
  WritablePacket *route_p = p->push(sizeof(struct dart_routing_header));
  
  header = (struct dart_routing_header *)route_p->data();
  memcpy(header->ident,ident,6);
  memcpy( header->_dst_nodeid, dst_nodeid, MAX_NODEID_LENTGH);
  header->_dst_nodeid_length = htons(dst_nodeid_length);

  memcpy( header->_src_nodeid, src_nodeid, MAX_NODEID_LENTGH);
  header->_src_nodeid_length = htons(src_nodeid_length);

  return(route_p);
}

struct dart_routing_header *
DartProtocol::get_route_header(Packet *p)
{
  return (struct dart_routing_header *)p->data();
}

click_ether *
DartProtocol::get_ether_header(Packet *p)
{
  return (click_ether *)&(p->data()[sizeof(struct dart_routing_header)]);
}

void
DartProtocol::strip_route_header(Packet *p)
{
  p->pull(sizeof(struct dart_routing_header));
}

CLICK_ENDDECLS

ELEMENT_PROVIDES(DartProtocol)

