#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <clicknet/ether.h>

#include "hawk_protocol.hh"
#include "hawk_routingpeek.hh"

CLICK_DECLS

HawkRoutingPeek::HawkRoutingPeek()
{
  RoutingPeek::init();
}

HawkRoutingPeek::~HawkRoutingPeek()
{
}

uint32_t
HawkRoutingPeek::get_all_header_len(Packet *)
{
  return (sizeof(click_brn) + sizeof(struct hawk_routing_header));
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RoutingPeek)
EXPORT_ELEMENT(HawkRoutingPeek)
