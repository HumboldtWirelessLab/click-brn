#include <click/config.h>
#include "rateselection.hh"

CLICK_DECLS

RateSelection::RateSelection()
{
  BRNElement::init();
}

RateSelection::~RateSelection()
{
}

void
RateSelection::add_handlers()
{
  BRNElement::add_handlers();
}

ELEMENT_PROVIDES(RateSelection)
CLICK_ENDDECLS
