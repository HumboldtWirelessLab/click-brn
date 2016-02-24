#include <click/config.h>
#include "rateselection.hh"

CLICK_DECLS

extern "C" {
  static int mcs_data_rate_sorter(const void *va, const void *vb, void * /*thunk*/) {
    const MCS *a = reinterpret_cast<const MCS *>(va);
    const MCS *b = reinterpret_cast<const MCS *>(vb);

    if ( a->_data_rate > b->_data_rate ) return 1;
    if ( a->_data_rate < b->_data_rate ) return -1;
    return 0;
  }
}

RateSelection::RateSelection()
{
  BRNElement::init();
}

RateSelection::~RateSelection()
{
}

void *
RateSelection::cast(const char *name)
{
  if (strcmp(name, "RateSelection") == 0)
    return dynamic_cast<RateSelection *>(this);
  else if (strcmp(name, "Scheme") == 0)
    return dynamic_cast<Scheme *>(this);
  else
    return NULL;
}

void
RateSelection::set_strategy(uint32_t strategy)
{
  _strategy = strategy;
}

bool
RateSelection::handle_strategy(uint32_t strategy)
{
  return (strategy == _default_strategy);
}

uint32_t
RateSelection::get_strategy()
{
  return _default_strategy;
}

void
RateSelection::sort_rates_by_data_rate(NeighbourRateInfo *nri)
{
  click_qsort(nri->_rates.begin(), nri->_rates.size(), sizeof(MCS), mcs_data_rate_sorter);
}

void
RateSelection::add_handlers()
{
  BRNElement::add_handlers();
}

ELEMENT_PROVIDES(RateSelection)
CLICK_ENDDECLS
