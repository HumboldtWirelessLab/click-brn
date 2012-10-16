#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include "routingalgorithm.hh"

CLICK_DECLS

RoutingAlgorithm::RoutingAlgorithm()
{
}

RoutingAlgorithm::~RoutingAlgorithm()
{
}

void
RoutingAlgorithm::init()
{
  BRNElement::init();
}

void
RoutingAlgorithm::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BRNElement)
ELEMENT_PROVIDES(RoutingPeek)
