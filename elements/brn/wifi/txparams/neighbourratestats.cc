#include <click/config.h>

#include "neighbourratestats.hh"

CLICK_DECLS

NeighbourRateStats::NeighbourRateStats()
{

}

NeighbourRateStats::~NeighbourRateStats()
{
}

void
NeighbourRateStats::set_next_timeslot()
{
  last_timeslot = curr_timeslot;
  curr_timeslot = (curr_timeslot + 1) % no_timeslots;
  cnt_updates++;

  for (RateStatsMapIter it = _ratestatsmap.begin(); it.live(); ++it) {
    MCS mcs = it.key();
    RateStats *rs = it.value();
    RateStats *current_rs = &(rs[last_timeslot]);

    /* stats for old */
    if (current_rs->count_tx > 0) {
      current_rs->psr = (100000 * current_rs->count_tx_succ)/current_rs->count_tx;

      if (current_rs->count_tx_with_rtscts > 0)
        current_rs->psr_with_rtscts = (100000 * current_rs->count_tx_with_rtscts_succ)/current_rs->count_tx_with_rtscts;

      if (current_rs->count_tx_wo_rtscts > 0)
        current_rs->psr_wo_rtscts = (100000 * current_rs->count_tx_wo_rtscts_succ)/current_rs->count_tx_wo_rtscts;
    }

    current_rs->throughput = current_rs->psr * mcs._data_rate;
    current_rs->throughput_with_rtscts = current_rs->psr_with_rtscts * mcs._data_rate;
    current_rs->throughput_wo_rtscts = current_rs->psr_wo_rtscts * mcs._data_rate;

    /* clear new */
    memset(&(rs[curr_timeslot]), 0, sizeof(RateStats));
  }
}

void
NeighbourRateStats::update_rate(MCS &mcs, int tries, int success, bool use_rts_cts)
{
  RateStats **rsp = _ratestatsmap.findp(mcs);
  RateStats *rs;
  RateStats *current_rs;

  //click_chatter("Update: %d %d %d",mcs._data_rate,tries,success);

  if ( rsp == NULL ) {
    rs = new RateStats[no_timeslots];
    memset(rs, 0, no_timeslots * sizeof(RateStats));
    _ratestatsmap.insert(mcs, rs);
  } else {
    rs = *rsp;
  }

  current_rs = &(rs[curr_timeslot]);

  current_rs->count_tx += tries;
  current_rs->count_tx_succ += success;

  if (use_rts_cts) {
    current_rs->count_tx_with_rtscts += tries;
    current_rs->count_tx_with_rtscts_succ += success;
  } else {
    current_rs->count_tx_wo_rtscts += tries;
    current_rs->count_tx_wo_rtscts_succ += success;
  }
}

RateStats*
NeighbourRateStats::get_last_ratestats(MCS &mcs)
{
  RateStats **rsp = _ratestatsmap.findp(mcs);

  if ( rsp == NULL ) return NULL;

  return &((*rsp)[last_timeslot]);
}

RateStats*
NeighbourRateStats::get_ratestats(MCS &mcs, int index)
{
  RateStats **rsp = _ratestatsmap.findp(mcs);

  if ( rsp == NULL ) return NULL;

  return &((*rsp)[(last_timeslot + no_timeslots + index)%no_timeslots]);
}


ELEMENT_PROVIDES(NeighbourRateStats)

CLICK_ENDDECLS
