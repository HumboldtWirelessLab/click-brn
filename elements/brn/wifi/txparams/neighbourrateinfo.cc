#include <click/config.h>
#include "neighbourrateinfo.hh"

CLICK_DECLS

NeighbourRateInfo::NeighbourRateInfo()
{
  _rs_data = NULL;
}

NeighbourRateInfo::~NeighbourRateInfo()
{
}

ELEMENT_PROVIDES(NeighbourRateInfo)
CLICK_ENDDECLS
