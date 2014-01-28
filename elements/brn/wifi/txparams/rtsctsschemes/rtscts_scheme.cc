#include <click/config.h>
#include "rtscts_scheme.hh"

CLICK_DECLS

RtsCtsScheme::RtsCtsScheme()
{
}

RtsCtsScheme::~RtsCtsScheme()
{
}

void RtsCtsScheme::handle_feedback()
{
}

void RtsCtsScheme::set_conf()
{
}

void RtsCtsScheme::set_strategy(uint32_t strategy)
{
  _strategy = strategy;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(RtsCtsScheme)

