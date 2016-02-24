#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "brn2_dsrprotocol.hh"

CLICK_DECLS

int
DSRProtocol::header_length(Packet *p) {
  const click_brn_dsr *dsr_header = reinterpret_cast<const click_brn_dsr *>((p->data()));

  return header_length(dsr_header);
}

int
DSRProtocol::header_length(const click_brn_dsr *brn_dsr) {
  return (sizeof(click_brn_dsr) + brn_dsr->dsr_hop_count * sizeof(click_dsr_hop) );
}

const click_dsr_hop*
DSRProtocol::get_hops(Packet *p) {
  return reinterpret_cast<const click_dsr_hop*>(p->data() + sizeof(click_brn_dsr));
}

click_dsr_hop*
DSRProtocol::get_hops(WritablePacket *p) {
  return reinterpret_cast<click_dsr_hop*>(p->data() + sizeof(click_brn_dsr));
}

const click_dsr_hop*
DSRProtocol::get_hops(const click_brn_dsr *brn_dsr) {
  return reinterpret_cast<const click_dsr_hop*>(&brn_dsr[1]);
}

click_dsr_hop*
DSRProtocol::get_hops(click_brn_dsr *brn_dsr) {
  return reinterpret_cast<click_dsr_hop*>(&brn_dsr[1]);
}

WritablePacket *
DSRProtocol::extend_hops(WritablePacket *p, int count) {
  WritablePacket *new_p = p->put(count * sizeof(click_dsr_hop));
  return new_p;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DSRProtocol)

