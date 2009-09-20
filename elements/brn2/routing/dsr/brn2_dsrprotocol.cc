#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "brn2_dsrprotocol.hh"

CLICK_DECLS

DSRProtocol::DSRProtocol()
{
}

DSRProtocol::~DSRProtocol()
{
}

int
DSRProtocol::header_length(Packet *p) {
  click_brn_dsr *dsr_header = (click_brn_dsr *)(p->data());

//  return ( sizeof(click_brn_dsr) + dsr_header->dsr_hop_count * sizeof(click_dsr_hop) );
  return (sizeof(click_brn_dsr));
}

click_dsr_hop*
DSRProtocol::get_hops(const Packet *p) {
//  return (click_dsr_hop*)(p->data() + sizeof(click_brn_dsr));
  click_brn_dsr *brn_dsr = (click_brn_dsr *)(p->data());
  return (brn_dsr->addr);
}

click_dsr_hop*
DSRProtocol::get_hops(click_brn_dsr *brn_dsr) {
//  void *p = (void*)brn_dsr;
//  return (p + sizeof(click_brn_dsr));

  return ( brn_dsr->addr);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DSRProtocol)

