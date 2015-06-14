#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "hawk_protocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
CLICK_DECLS

WritablePacket *
HawkProtocol::add_route_header(uint8_t *dst_nodeid, uint8_t *src_nodeid, Packet *p)
{
  return add_route_header(dst_nodeid, src_nodeid, dst_nodeid, NULL, p);
}


WritablePacket *
HawkProtocol::add_route_header(uint8_t *dst_nodeid, uint8_t *src_nodeid,
                               uint8_t *next_nodeid, EtherAddress *_next, Packet *p)
{
  struct hawk_routing_header *header;
  WritablePacket *route_p = p->push(sizeof(struct hawk_routing_header));

  header = (struct hawk_routing_header *)route_p->data();

  memcpy( header->_dst_nodeid, dst_nodeid, MAX_NODEID_LENTGH);
  memcpy( header->_src_nodeid, src_nodeid, MAX_NODEID_LENTGH);
  memcpy( header->_next_nodeid, next_nodeid, MAX_NODEID_LENTGH);
  header->_metric = 0;
  if (_next != NULL)
    memcpy(header->_next_etheraddress, _next->data(), 6);
  else
    memset(header->_next_etheraddress, 0, 6);

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

void
HawkProtocol::set_next_hop(Packet *p, EtherAddress *next,uint8_t* next_nodeid)
{
  struct hawk_routing_header *rh = (struct hawk_routing_header *)p->data();
  memcpy( rh->_next_etheraddress, next->data(), 6);
  memcpy( rh->_next_nodeid, next_nodeid, MAX_NODEID_LENTGH);
}

bool
HawkProtocol::has_next_hop(Packet *p)
{
  uint8_t emptyNext[] = { 0, 0, 0, 0, 0, 0 };
  struct hawk_routing_header *rh = (struct hawk_routing_header *)p->data();
  return ( memcmp( rh->_next_etheraddress, emptyNext, 6 ) != 0 );
}

void
HawkProtocol::clear_next_hop(Packet *p)
{
  struct hawk_routing_header *rh = (struct hawk_routing_header *)p->data();
  memset( rh->_next_etheraddress, 0, 6 );
}
void
HawkProtocol::add_metric(Packet *p,uint8_t metric)
{
  struct hawk_routing_header *rh = (struct hawk_routing_header *)p->data();
  rh->_metric += metric;
}

void
HawkProtocol::set_rew_metric(Packet *p,uint8_t metric)
{
  struct hawk_routing_header *rh = (struct hawk_routing_header *)p->data();
  rh->_rew_metric = metric;
}

CLICK_ENDDECLS

ELEMENT_PROVIDES(HawkProtocol)

