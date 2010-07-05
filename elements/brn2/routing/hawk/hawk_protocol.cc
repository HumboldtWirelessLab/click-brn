#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "hawk_protocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
CLICK_DECLS


WritablePacket *
HawkProtocol::add_route_header(uint8_t *dst_nodeid, int dst_nodeid_length,
                                  uint8_t *src_nodeid, int src_nodeid_length, Packet *p)
{
  struct hawk_routing_header *header;
  WritablePacket *route_p = p->push(sizeof(struct hawk_routing_header));

  header = (struct hawk_routing_header *)route_p->data();

  memcpy( header->_dst_nodeid, dst_nodeid, MAX_NODEID_LENTGH);
  header->_dst_nodeid_length = htonl(dst_nodeid_length);

  memcpy( header->_src_nodeid, src_nodeid, MAX_NODEID_LENTGH);
  header->_src_nodeid_length = htonl(src_nodeid_length);

  return(route_p);
}

struct hawk_routing_header *
HawkProtocol::get_route_header(Packet *p)
{
  return (struct hawk_routing_header *)p->data();
}

click_ether *
HawkProtocol::get_ether_header(Packet *p)
{
  return (click_ether *)&(p->data()[sizeof(struct hawk_routing_header)]);
}

void
HawkProtocol::strip_route_header(Packet *p)
{
  p->pull(sizeof(struct hawk_routing_header));
}

CLICK_ENDDECLS

ELEMENT_PROVIDES(HawkProtocol)

