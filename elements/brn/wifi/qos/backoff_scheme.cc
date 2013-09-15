#include <click/config.h>
#include "backoff_scheme.hh"

CLICK_DECLS



BackoffScheme::BackoffScheme()
  : _min_cwmin(0),
    _max_cwmin(0)
{
}


BackoffScheme::~BackoffScheme()
{
}


void BackoffScheme::set_conf(uint32_t min, uint32_t max)
{
  _min_cwmin = min;
  _max_cwmin = max;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(BackoffScheme)

