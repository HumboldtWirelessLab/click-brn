#include <click/config.h>
#include <click/straccum.hh>

#include "neighbourrateinfo.hh"

#define NUM_TIME_SLOTS 5


CLICK_DECLS

int NeighbourRateInfo::_debug = 2;


NeighbourRateInfo::NeighbourRateInfo()
{
  _rs_data = NULL;

  init_stamp     = Timestamp::now();
  curr_timeslot  = 0;
  timeslots      = new NeighbourRateStats[NUM_TIME_SLOTS];
  time_intervall = 100;
}



NeighbourRateInfo::~NeighbourRateInfo()
{
}




NeighbourRateInfo::NeighbourRateInfo(EtherAddress eth, Vector<MCS> rates, uint8_t max_power)
{
  _eth           = eth;
  _rates         = rates;
  _max_power     = max_power;
  _power         = max_power;
  _rs_data       = NULL;

  init_stamp     = Timestamp::now();
  curr_timeslot  = 0;
  timeslots      = new NeighbourRateStats[NUM_TIME_SLOTS];
  time_intervall = 100;
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
NeighbourRateInfo::get_current_timeslot()
{
  Timestamp current_stamp;
  Timestamp diff_stamp;


  int32_t time_diff;
  int new_timeslot;


  current_stamp = Timestamp::now();
  diff_stamp    = current_stamp - init_stamp;


  /* get time diff in milsec */
  time_diff = diff_stamp.msecval();


  /* calculate new time slot based on the time diff, the time slot intervall size
   * and the number of timeslots we are using */
  new_timeslot = time_diff / time_intervall;
  new_timeslot = ((int) new_timeslot) % NUM_TIME_SLOTS;

  curr_timeslot = new_timeslot;

  return curr_timeslot;
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

  for (int i = 0; i < _rates.size(); i++) {
    click_chatter("%d", _rates[i]._data_rate);
    click_chatter("%d", _rates[i]._is_ht);
    click_chatter("%d", i);
    click_chatter("\n");
  }
}







ELEMENT_PROVIDES(NeighbourRateInfo)
ELEMENT_REQUIRES(NeighbourRateStats)

CLICK_ENDDECLS
