#include <click/config.h>
#include "backoff_scheme.hh"

CLICK_DECLS



BackoffScheme::BackoffScheme()
  : _id(0)
{
}

BackoffScheme::~BackoffScheme()
{
}


void BackoffScheme::set_scheme_id(uint16_t id)
{
  _id = id;
}



CLICK_ENDDECLS
ELEMENT_PROVIDES(BackoffScheme)

