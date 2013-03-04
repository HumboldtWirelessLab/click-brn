#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <clicknet/ether.h>

#include "dart_protocol.hh"
#include "dart_routingpeek.hh"

CLICK_DECLS

DartRoutingPeek::DartRoutingPeek()
{
  RoutingPeek::init();
}

DartRoutingPeek::~DartRoutingPeek()
{
}

uint32_t
DartRoutingPeek::get_all_header_len(Packet *)
{
  return (sizeof(click_brn) + sizeof(struct dart_routing_header));
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RoutingPeek)
EXPORT_ELEMENT(DartRoutingPeek)
