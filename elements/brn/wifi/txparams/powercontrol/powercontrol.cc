#include <click/config.h>
#include "powercontrol.hh"

CLICK_DECLS

PowerControl::PowerControl()
{
  BRNElement::init();
}

PowerControl::~PowerControl()
{
}

void *
PowerControl::cast(const char *name)
{
  if (strcmp(name, "PowerControl") == 0)
    return dynamic_cast<PowerControl *>(this);
  else if (strcmp(name, "Scheme") == 0)
    return dynamic_cast<Scheme *>(this);
  else
    return NULL;
}

void
PowerControl::set_strategy(uint32_t strategy)
{
  _strategy = strategy;
}

bool
PowerControl::handle_strategy(uint32_t strategy)
{
  return (strategy == _default_strategy);
}

uint32_t
PowerControl::get_strategy()
{
  return _default_strategy;
}

void
PowerControl::add_handlers()
{
  BRNElement::add_handlers();
}

ELEMENT_PROVIDES(PowerControl)
CLICK_ENDDECLS
