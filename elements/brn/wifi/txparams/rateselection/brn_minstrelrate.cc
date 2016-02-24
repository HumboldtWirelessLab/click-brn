#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <elements/wifi/availablerates.hh>

#include "elements/brn/brn2.h"
#include "rateselection.hh"
#include "brn_minstrelrate.hh"

CLICK_DECLS

#define CREDITS_FOR_RAISE 10
#define STEPUP_RETRY_THRESHOLD 10

BrnMinstrelRate::BrnMinstrelRate():
  alpha(RS_MINSTREL_DEFAULT_ALPHA)
{
  _default_strategy = RATESELECTION_MINSTREL;
}

void *
BrnMinstrelRate::cast(const char *name)
{
  if (strcmp(name, "BrnMinstrelRate") == 0)
    return dynamic_cast<BrnMinstrelRate *>(this);

  return RateSelection::cast(name);
}

BrnMinstrelRate::~BrnMinstrelRate()
{
}

int
BrnMinstrelRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int ret = Args(conf, this, errh)
      .read("DEBUG", _debug)
      .complete();
  return ret;

  click_brn_srandom();
}

/************************************************************************************/
/********************************** H E L P E R *************************************/
/************************************************************************************/


/************************************************************************************/
/*********************************** M A I N ****************************************/
/************************************************************************************/

void
BrnMinstrelRate::adjust_all(NeighborTable *neighbors)
{
  for (NIter iter = neighbors->begin(); iter.live(); ++iter) {
    NeighbourRateInfo *nri = iter.value();
    setMinstrelInfo(nri);
  }
}

void
BrnMinstrelRate::process_feedback(struct rateselection_packet_info */*rs_pkt_info*/, NeighbourRateInfo * /*nri*/)
{
  return;
}

void
BrnMinstrelRate::assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *nri)
{
  click_wifi_extra *eh = rs_pkt_info->ceh;

  int sample_prob = (nri->stats.cnt_updates == 0)?0:(click_random() % 100);

  if (nri->_rs_data == NULL) setMinstrelInfo(nri);

  MinstrelNodeInfo *mni = reinterpret_cast<MinstrelNodeInfo*>(nri->_rs_data);


  MCS sample_mcs;

  if (sample_prob < 10) {
    do {
      sample_mcs = nri->_rates[click_random()%nri->_rates.size()];
    } while ((sample_mcs == mni->best_eff_tp) || (sample_mcs == mni->second_eff_tp) || (sample_mcs == mni->best_psr) || (sample_mcs == mni->lowest_rate));

    //click_chatter("%d %d %d", mni->best_eff_tp._data_rate, mni->best_eff_tp_raw, mni->best_eff_tp_psr );

    if ( sample_mcs._data_rate > mni->best_eff_tp_raw/*mni->best_eff_tp._data_rate*/ ) { //compare best eff tp with possible tp of sampling rate (=MCS-rate)
      sample_mcs.setWifiRate(eh,0);
      mni->best_eff_tp.setWifiRate(eh,1);
    } else {
      mni->best_eff_tp.setWifiRate(eh,0);
      sample_mcs.setWifiRate(eh,1);
    }
  } else {
    mni->best_eff_tp.setWifiRate(eh,0);
    mni->second_eff_tp.setWifiRate(eh,1);
  }

  mni->best_psr.setWifiRate(eh,2);
  mni->lowest_rate.setWifiRate(eh,3);

  eh->max_tries = 3;
  eh->max_tries1 = 3;
  eh->max_tries2 = 3;
  eh->max_tries3 = 3;

  return;
}

void
BrnMinstrelRate::setMinstrelInfo(NeighbourRateInfo *nri)
{
  BRN_DEBUG("Set MinstrelNodeInfo");
  if ( nri->_rs_data == NULL ) nri->_rs_data = reinterpret_cast<void*>(new MinstrelNodeInfo());
  MinstrelNodeInfo *mni = reinterpret_cast<MinstrelNodeInfo*>(nri->_rs_data);

  if (nri->stats._ratestatsmap.size() == 0) {
    mni->best_eff_tp = nri->_rates[0];
    mni->best_eff_tp_raw = 0;
    mni->best_eff_tp_psr = 0;

    mni->second_eff_tp = nri->_rates[0];

    mni->best_psr = nri->_rates[0];

    mni->lowest_rate = nri->_rates[0];
    mni->second_lowest_rate = nri->_rates[1];

    BRN_DEBUG("no rate TP: %d PSR: %d LR: %d", mni->best_eff_tp._data_rate, mni->best_psr._data_rate, mni->lowest_rate._data_rate);
    return;
  }

  uint32_t best_psr = 0;
  uint32_t best_psr_rate = 0;
  MCS best_psr_mcs;

  uint32_t best_tp = 0;
  MCS best_tp_mcs;

  uint32_t second_tp = 0;
  MCS second_tp_mcs;

  uint32_t lowest_datarate = 0;
  MCS lowest_datarate_mcs;

  uint32_t second_lowest_datarate = 0;
  MCS second_lowest_datarate_mcs;

  for (RateStatsMapIter it = nri->stats._ratestatsmap.begin(); it.live(); ++it) {
    MCS mcs = it.key();

    RateStats* rs = nri->stats.get_ratestats(mcs,-2);
    /* TODO: psr for rts,... */
    uint32_t psr = rs->psr;

    for ( int i = -1; i <= 0; i++ ) {
      RateStats* rs = nri->stats.get_ratestats(mcs,i);
      psr = (psr*(alpha) + rs->psr*(100-alpha))/100;
    }

    uint32_t tp = psr * mcs._data_rate;

    if ((psr > best_psr) || ((psr == best_psr) && (psr != 0) && (mcs._data_rate > best_psr_rate))) {
      best_psr = psr;
      best_psr_mcs = mcs;
      best_psr_rate = mcs._data_rate;
    }

    if ( tp > best_tp ) {
      second_tp = best_tp;
      second_tp_mcs = best_tp_mcs;
      best_tp = tp;
      best_tp_mcs = mcs;
      mni->best_eff_tp_raw = tp / 100000;
      mni->best_eff_tp_psr = psr / 1000;
    } else {
      if ( tp > second_tp ) {
        second_tp = tp;
        second_tp_mcs = mcs;
      }
    }

    if ( (lowest_datarate == 0) || (lowest_datarate > mcs._data_rate) ) {
      second_lowest_datarate = lowest_datarate;
      second_lowest_datarate_mcs = lowest_datarate_mcs;
      lowest_datarate = mcs._data_rate;
      lowest_datarate_mcs = mcs;
    } else if ( (second_lowest_datarate == 0) || (second_lowest_datarate > mcs._data_rate) ) {
      second_lowest_datarate = mcs._data_rate;
      second_lowest_datarate_mcs = mcs;
    }
  }

  mni->best_eff_tp = best_tp_mcs;
  mni->second_eff_tp = second_tp_mcs;
  mni->lowest_rate = lowest_datarate_mcs;

  if ((best_psr_mcs == best_tp_mcs) || (best_psr_mcs == second_tp_mcs ) || (best_psr_mcs == lowest_datarate_mcs) ) mni->best_psr = second_lowest_datarate_mcs;
  else mni->best_psr = best_psr_mcs;

  BRN_DEBUG("TP: %d 2.TP: %d PSR: %d LR: %d", mni->best_eff_tp._data_rate, mni->second_eff_tp._data_rate, mni->best_psr._data_rate, mni->lowest_rate._data_rate);
}


String
BrnMinstrelRate::print_neighbour_info(NeighbourRateInfo * /*nri*/, int /*tabs*/)
{
  StringAccum sa;

  return sa.take_string();
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

void
BrnMinstrelRate::add_handlers()
{
  RateSelection::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnMinstrelRate)

