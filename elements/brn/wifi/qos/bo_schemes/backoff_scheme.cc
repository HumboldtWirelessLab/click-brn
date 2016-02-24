#include <click/config.h>
#include "backoff_scheme.hh"

CLICK_DECLS

BackoffScheme::BackoffScheme()
  : _min_cwmin(BACKOFF_SCHEME_MIN_CWMIN),
    _max_cwmin(BACKOFF_SCHEME_MAX_CWMAX)
{
}

BackoffScheme::~BackoffScheme()
{
}

void *
BackoffScheme::cast(const char *name)
{
  if (strcmp(name, "BackoffScheme") == 0)
    return dynamic_cast<BackoffScheme *>(this);
  else if (strcmp(name, "Scheme") == 0)
    return dynamic_cast<Scheme *>(this);
  else
    return NULL;
}

void
BackoffScheme::handle_feedback(uint8_t retries)
{
  (void)retries;
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

