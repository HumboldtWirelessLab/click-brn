#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include "routing_peek.hh"

CLICK_DECLS

RoutingPeek::RoutingPeek()
{
}

RoutingPeek::~RoutingPeek()
{
}

void
RoutingPeek::init()
{
  BRNElement::init();
}

static String
read_peek_info(Element *e, void */*thunk*/)
{
  RoutingPeek *rp = (RoutingPeek *)e;
  return "Number of peek: " + String(rp->_peeklist.size()) + "\n";
}

void
RoutingPeek::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("peek_info", read_peek_info, (void *) 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BRNElement)
ELEMENT_PROVIDES(RoutingPeek)
