#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <clicknet/ether.h>

#include "brn2_dsrencap.hh"
#include "brn2_dsrdecap.hh"

#include "brn2_dsrpeek.hh"

CLICK_DECLS

DSRPeek::DSRPeek()
{
  RoutingPeek::init();
}

DSRPeek::~DSRPeek()
{
}

uint32_t
DSRPeek::get_all_header_len(Packet *packet)
{
  click_brn_dsr *dsr_src = (click_brn_dsr *)(packet->data() + sizeof(click_brn));
  return (sizeof(click_brn) + DSRProtocol::header_length(dsr_src));
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RoutingPeek)
EXPORT_ELEMENT(DSRPeek)
