#include <click/config.h>
#include "floodingpolicy.hh"

CLICK_DECLS

FloodingPolicy::FloodingPolicy():
  _flooding(NULL)
{
}

FloodingPolicy::~FloodingPolicy()
{
}

ELEMENT_PROVIDES(FloodingPolicy)
CLICK_ENDDECLS
