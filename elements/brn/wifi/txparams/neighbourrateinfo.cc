#include <click/config.h>
#include <click/straccum.hh>

#include "neighbourrateinfo.hh"

#define NUM_TIME_SLOTS 5


CLICK_DECLS

int NeighbourRateInfo::_debug = 2;


NeighbourRateInfo::NeighbourRateInfo() :
  _max_power(0),
  _rs_data(NULL),
  _pc_data(NULL),
  init_stamp(Timestamp::now()),
  stats_duration(0)
{
}

NeighbourRateInfo::~NeighbourRateInfo()
{
}

NeighbourRateInfo::NeighbourRateInfo(EtherAddress eth, Vector<MCS> rates, uint8_t max_power): _eth(eth), _rates(rates),
                                                                                              _max_power(max_power), _rs_data(NULL),
                                                                                              _pc_data(NULL), init_stamp(Timestamp::now()), stats_duration(0)
{
  stats.no_timeslots = 10;
  stats.curr_timeslot = 0;
  stats.last_timeslot = 0;
}



int
NeighbourRateInfo::rate_index(MCS rate)
{
  int ndx = -1;

    for (int x = 0; x < _rates.size(); x++) {
      if (rate.equals(_rates[x])) {
         ndx = x;
         break;
      }
    }

  return (ndx == _rates.size()) ? -1 : ndx;
}



int
NeighbourRateInfo::rate_index(uint32_t rate)
{
  int ndx = -1;

  for (int x = 0; x < _rates.size(); x++) {
    if ((!_rates[x]._is_ht) && (_rates[x]._rate == rate)) {
      ndx = x;
      break;
    }
  }

  return (ndx == _rates.size()) ? -1 : ndx;
}



MCS
NeighbourRateInfo::pick_rate(uint32_t index)
{
  if (_rates.size() == 0) {
    return MCS(2);
  }

  if (index > 0 && index < (uint32_t)_rates.size()) {
    return _rates[index];
  }

  return _rates[0];
}



bool
NeighbourRateInfo::is_ht(uint32_t rate)
{
  for (int i = 0; i < _rates.size(); i++) {
    if (_rates[i]._rate == rate)
      return _rates[i]._is_ht;
  }

  return false;
}

int
NeighbourRateInfo::get_rate_index(uint32_t rate)
{
  int ndx = -1;

  for (int i = 0; i < _rates.size(); i++) {
    if (_rates[i]._rate == rate)
      ndx = i;
  }

  return ndx;
}

void
NeighbourRateInfo::print_mcs_vector()
{
  click_chatter("Index\tRate\tHT");
  for (int i = 0; i < _rates.size(); i++)
    click_chatter("%d\t%d\t%d", i, _rates[i]._data_rate, _rates[i]._is_ht);
}

ELEMENT_PROVIDES(NeighbourRateInfo)
ELEMENT_REQUIRES(NeighbourRateStats)

CLICK_ENDDECLS
