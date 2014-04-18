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

void *
BackoffScheme::cast(const char *name)
{
  if (strcmp(name, "BackoffScheme") == 0)
    return (BackoffScheme *) this;
  else if (strcmp(name, "Scheme") == 0)
    return (Scheme *) this;
  else
    return NULL;
}

void BackoffScheme::set_conf(uint32_t min, uint32_t max)
{
  _min_cwmin = min;
  _max_cwmin = max;
}


void BackoffScheme::set_strategy(uint32_t strategy)
{
  _strategy = strategy;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(BackoffScheme)

