#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "brn2_dsrprotocol.hh"

CLICK_DECLS

int
DSRProtocol::header_length(Packet *p) {
  click_brn_dsr *dsr_header = (click_brn_dsr *)(p->data());

  return header_length(dsr_header);
}

int
DSRProtocol::header_length(click_brn_dsr *brn_dsr) {
  return (sizeof(click_brn_dsr) + brn_dsr->dsr_hop_count * sizeof(click_dsr_hop) );
}

click_dsr_hop*
DSRProtocol::get_hops(const Packet *p) {
  return (click_dsr_hop*)(p->data() + sizeof(click_brn_dsr));
}

click_dsr_hop*
DSRProtocol::get_hops(click_brn_dsr *brn_dsr) {
  void *p = (void*)&brn_dsr[1];  //return pointer after click_brn_dsr (hops follows the header)
  return ((click_dsr_hop*)p);
}

WritablePacket *
DSRProtocol::extend_hops(WritablePacket *p, int count) {
  WritablePacket *new_p = p->put(count * sizeof(click_dsr_hop));
  return new_p;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DSRProtocol)

