#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "georprotocol.hh"

CLICK_DECLS

GeorProtocol::GeorProtocol()
{
}

GeorProtocol::~GeorProtocol()
{
}

WritablePacket *
GeorProtocol::addRoutingHeader(Packet *p,
                               EtherAddress *sea, GPSPosition *spos,
                               EtherAddress *dea, GPSPosition *dpos)
{
  WritablePacket *pout = p->push(sizeof(struct geor_header));

  if ( pout ) {
    struct geor_header *georh = (struct geor_header *)pout->data();
    georh->type = GEOR_TYPE_ROUTING;
    memcpy(georh->src,sea->data(),6);
    spos->getPosition(&(georh->src_pos));
    memcpy(georh->dst,dea->data(),6);
    dpos->getPosition(&(georh->dst_pos));

  }

  return pout;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(GeorProtocol)
