#ifndef RATESSTATS_HH
#define RATESSTATS_HH
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/bighashmap.hh>
#include <click/timestamp.hh>

#include "elements/brn/wifi/brnavailablerates.hh"

CLICK_DECLS

 typedef struct rate_stats {
  int count_tx;
  int count_tx_succ;

  int count_tx_with_rtscts;
  int count_tx_with_rtscts_succ;

  int count_tx_wo_rtscts;
  int count_tx_wo_rtscts_succ;

  int psr;
  int psr_with_rtscts;
  int psr_wo_rtscts;

  int throughput;
  int throughput_with_rtscts;
  int throughput_wo_rtscts;

} RateStats;

typedef HashMap<MCS, RateStats*> RateStatsMap;
typedef RateStatsMap::const_iterator RateStatsMapIter;

class NeighbourRateStats {

  public:

    NeighbourRateStats();
    ~NeighbourRateStats();

    RateStatsMap _ratestatsmap;

    int no_timeslots;
    int curr_timeslot;
    int last_timeslot;

    int cnt_updates;

    void set_next_timeslot();
    void update_rate(MCS &mcs, int tries, int success, bool use_rts_cts);

    RateStats* get_last_ratestats(MCS &mcs);

    RateStats* get_ratestats(MCS &mcs, int index);


};


CLICK_ENDDECLS
#endif
