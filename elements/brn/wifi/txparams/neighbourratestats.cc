#include <click/config.h>

#include "neighbourratestats.hh"

CLICK_DECLS

NeighbourRateStats::NeighbourRateStats()
{
  per      = 0;
  throuput = 0;
  tries    = 0;
}


NeighbourRateStats::NeighbourRateStats(int per, int tp, int tries)
{
  this->per         = per;
  this->throuput    = tp;
  this->tries       = tries;
}


NeighbourRateStats::~NeighbourRateStats()
{
}

ELEMENT_PROVIDES(NeighbourRateStats)

CLICK_ENDDECLS
