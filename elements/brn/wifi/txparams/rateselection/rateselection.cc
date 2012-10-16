#include <click/config.h>
#include "rateselection.hh"

CLICK_DECLS

extern "C" {
  static int mcs_data_rate_sorter(const void *va, const void *vb, void */*thunk*/) {
    MCS *a = ((MCS *)va), *b = ((MCS *)vb);

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
