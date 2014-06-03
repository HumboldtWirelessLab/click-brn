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

BrnMinstrelRate::BrnMinstrelRate()
{
  mcs_zero = MCS(0);
  _default_strategy = RATESELECTION_MINSTREL;
}

void *
BrnMinstrelRate::cast(const char *name)
{
  if (strcmp(name, "BrnMinstrelRate") == 0)
    return (BrnMinstrelRate *) this;

  return RateSelection::cast(name);
}

BrnMinstrelRate::~BrnMinstrelRate()
{
}

int
BrnMinstrelRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
//  _alt_rate = false;
  _period = 1000;
  int ret = Args(conf, this, errh)
      //.read("ALT_RATE", _alt_rate)
      .read("PERIOD", _period)
      .read("DEBUG", _debug)
      .complete();
  return ret;
}

/************************************************************************************/
/********************************** H E L P E R *************************************/
/************************************************************************************/

/* convert mac80211 rate index to local array index */
/*static inline int32_t rix_to_ndx(struct minstrel_sta_info *mi, int32_t rix)
{
  int32_t i = rix;
  for (i = rix; i >= 0; i--)
    if (mi->r[i].rix == rix)
      break;
  return i;
}
*/
/****
static void minstrel_update_stats(struct minstrel_priv *mp, struct minstrel_sta_info *mi)
{
  u32 max_tp = 0, index_max_tp = 0, index_max_tp2 = 0;
  u32 max_prob = 0, index_max_prob = 0;
  u32 usecs;
  u32 p;
  int i;

  mi->stats_update = Timestamp::now();
  for (i = 0; i < mi->n_rates; i++) {
    struct minstrel_rate *mr = &mi->r[i];

    usecs = mr->perfect_tx_time;
    if (!usecs)
      usecs = 1000000;
****/
    /* To avoid rounding issues, probabilities scale from 0 (0%)
    * to 18000 (100%) */
/****
    if (mr->attempts) {
      p = (mr->success * 18000) / mr->attempts;
      mr->succ_hist += mr->success;
      mr->att_hist += mr->attempts;
      mr->cur_prob = p;
      p = ((p * (100 - mp->ewma_level)) + (mr->probability *
          mp->ewma_level)) / 100;
      mr->probability = p;
      mr->cur_tp = p * (1000000 / usecs);
    }

    mr->last_success = mr->success;
    mr->last_attempts = mr->attempts;
    mr->success = 0;
    mr->attempts = 0;
****/

    /* Sample less often below the 10% chance of success.
    * Sample less often above the 95% chance of success. */
    /****
        if ((mr->probability > 17100) || (mr->probability < 1800)) {
      mr->adjusted_retry_count = mr->retry_count >> 1;
      if (mr->adjusted_retry_count > 2)
        mr->adjusted_retry_count = 2;
      mr->sample_limit = 4;
    } else {
      mr->sample_limit = -1;
      mr->adjusted_retry_count = mr->retry_count;
    }
    if (!mr->adjusted_retry_count)
      mr->adjusted_retry_count = 2;
  }

  for (i = 0; i < mi->n_rates; i++) {
    struct minstrel_rate *mr = &mi->r[i];
    if (max_tp < mr->cur_tp) {
      index_max_tp = i;
      max_tp = mr->cur_tp;
    }
    if (max_prob < mr->probability) {
      index_max_prob = i;
      max_prob = mr->probability;
    }
  }

  max_tp = 0;
  for (i = 0; i < mi->n_rates; i++) {
    struct minstrel_rate *mr = &mi->r[i];

    if (i == index_max_tp)
      continue;

    if (max_tp < mr->cur_tp) {
      index_max_tp2 = i;
      max_tp = mr->cur_tp;
    }
  }
  mi->max_tp_rate = index_max_tp;
  mi->max_tp_rate2 = index_max_tp2;
  mi->max_prob_rate = index_max_prob;
}

static void
    minstrel_tx_status(void *priv, struct ieee80211_supported_band *sband,
                       struct ieee80211_sta *sta, void *priv_sta,
                       struct sk_buff *skb)
{
  struct minstrel_sta_info *mi = priv_sta;
  struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
  struct ieee80211_tx_rate *ar = info->status.rates;
  int i, ndx;
  int success;

  success = !!(info->flags & IEEE80211_TX_STAT_ACK);

  for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
    if (ar[i].idx < 0)
      break;

    ndx = rix_to_ndx(mi, ar[i].idx);
    if (ndx < 0)
      continue;

    mi->r[ndx].attempts += ar[i].count;

    if ((i != IEEE80211_TX_MAX_RATES - 1) && (ar[i + 1].idx < 0))
      mi->r[ndx].success += success;
  }

  if ((info->flags & IEEE80211_TX_CTL_RATE_CTRL_PROBE) && (i >= 0))
    mi->sample_count++;

  if (mi->sample_deferred > 0)
    mi->sample_deferred--;
}


static inline unsigned int
minstrel_get_retry_count(struct minstrel_rate *mr, struct ieee80211_tx_info *info)
{
  unsigned int retry = mr->adjusted_retry_count;

  if (info->control.rates[0].flags & IEEE80211_TX_RC_USE_RTS_CTS)
    retry = MAX(2U, MIN(mr->retry_count_rtscts, retry));
  else if (info->control.rates[0].flags & IEEE80211_TX_RC_USE_CTS_PROTECT)
    retry = MAX(2U, MIN(mr->retry_count_cts, retry));
  return retry;
}


static int
minstrel_get_next_sample(struct minstrel_sta_info *mi)
{
  unsigned int sample_ndx;
  sample_ndx = SAMPLE_TBL(mi, mi->sample_idx, mi->sample_column);
  mi->sample_idx++;
  if ((int) mi->sample_idx > (mi->n_rates - 2)) {
    mi->sample_idx = 0;
    mi->sample_column++;
    if (mi->sample_column >= SAMPLE_COLUMNS)
      mi->sample_column = 0;
  }
  return sample_ndx;
}

static void
minstrel_get_rate(void *priv, struct ieee80211_sta *sta, void *priv_sta, struct ieee80211_tx_rate_control *txrc)
{
  struct sk_buff *skb = txrc->skb;
  struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
  struct minstrel_sta_info *mi = priv_sta;
  struct minstrel_priv *mp = priv;
  struct ieee80211_tx_rate *ar = info->control.rates;
  unsigned int ndx, sample_ndx = 0;
  bool mrr;
  bool sample_slower = false;
  bool sample = false;
  int i, delta;
  int mrr_ndx[3];
  int sample_rate;

  if (rate_control_send_low(sta, priv_sta, txrc))
    return;

  mrr = mp->has_mrr && !txrc->rts && !txrc->bss_conf->use_cts_prot;

  if (time_after(jiffies, mi->stats_update + (mp->update_interval *
      HZ) / 1000))
    minstrel_update_stats(mp, mi);

  ndx = mi->max_tp_rate;

  if (mrr)
    sample_rate = mp->lookaround_rate_mrr;
  else
    sample_rate = mp->lookaround_rate;

  mi->packet_count++;
  delta = (mi->packet_count * sample_rate / 100) -
      (mi->sample_count + mi->sample_deferred / 2);
    ****/

  /* delta > 0: sampling required */
  /****
      if ((delta > 0) && (mrr || !mi->prev_sample)) {
    struct minstrel_rate *msr;
    if (mi->packet_count >= 10000) {
      mi->sample_deferred = 0;
      mi->sample_count = 0;
      mi->packet_count = 0;
    } else if (delta > mi->n_rates * 2) {
  ****/
      /* With multi-rate retry, not every planned sample
      * attempt actually gets used, due to the way the retry
      * chain is set up - [max_tp,sample,prob,lowest] for
      * sample_rate < max_tp.
      *
      * If there's too much sampling backlog and the link
      * starts getting worse, minstrel would start bursting
      * out lots of sampling frames, which would result
      * in a large throughput loss. */
     /****
          mi->sample_count += (delta - mi->n_rates * 2);
    }

    sample_ndx = minstrel_get_next_sample(mi);
    msr = &mi->r[sample_ndx];
    sample = true;
    sample_slower = mrr && (msr->perfect_tx_time >
        mi->r[ndx].perfect_tx_time);

    if (!sample_slower) {
      if (msr->sample_limit != 0) {
        ndx = sample_ndx;
        mi->sample_count++;
        if (msr->sample_limit > 0)
          msr->sample_limit--;
      } else {
        sample = false;
      }
    } else {
     ****/
      /* Only use IEEE80211_TX_CTL_RATE_CTRL_PROBE to mark
      * packets that have the sampling rate deferred to the
      * second MRR stage. Increase the sample counter only
      * if the deferred sample rate was actually used.
      * Use the sample_deferred counter to make sure that
      * the sampling is not done in large bursts */
      /****
          info->flags |= IEEE80211_TX_CTL_RATE_CTRL_PROBE;
      mi->sample_deferred++;
    }
  }
  mi->prev_sample = sample;
      ****/

  /* If we're not using MRR and the sampling rate already
  * has a probability of >95%, we shouldn't be attempting
  * to use it, as this only wastes precious airtime */
  /***
      if (!mrr && sample && (mi->r[ndx].probability > 17100))
    ndx = mi->max_tp_rate;

  ar[0].idx = mi->r[ndx].rix;
  ar[0].count = minstrel_get_retry_count(&mi->r[ndx], info);

  if (!mrr) {
    if (!sample)
      ar[0].count = mp->max_retry;
    ar[1].idx = mi->lowest_rix;
    ar[1].count = mp->max_retry;
    return;
  }
  ****/

  /* MRR setup */
  /****
      if (sample) {
    if (sample_slower)
      mrr_ndx[0] = sample_ndx;
    else
      mrr_ndx[0] = mi->max_tp_rate;
  } else {
    mrr_ndx[0] = mi->max_tp_rate2;
  }
  mrr_ndx[1] = mi->max_prob_rate;
  mrr_ndx[2] = 0;
  for (i = 1; i < 4; i++) {
    ar[i].idx = mi->r[mrr_ndx[i - 1]].rix;
    ar[i].count = mi->r[mrr_ndx[i - 1]].adjusted_retry_count;
  }
}


static void calc_rate_durations(struct minstrel_sta_info *mi, struct ieee80211_local *local,
                                struct minstrel_rate *d, struct ieee80211_rate *rate)
{
  int erp = !!(rate->flags & IEEE80211_RATE_ERP_G);

  d->perfect_tx_time = ieee80211_frame_duration(local, 1200,
      rate->bitrate, erp, 1);
  d->ack_time = ieee80211_frame_duration(local, 10,
                                         rate->bitrate, erp, 1);
}

static void init_sample_table(struct minstrel_sta_info *mi)
{
  unsigned int i, col, new_idx;
  unsigned int n_srates = mi->n_rates - 1;
  u8 rnd[8];

  mi->sample_column = 0;
  mi->sample_idx = 0;
  memset(mi->sample_table, 0, SAMPLE_COLUMNS * mi->n_rates);

  for (col = 0; col < SAMPLE_COLUMNS; col++) {
    for (i = 0; i < n_srates; i++) {
      get_random_bytes(rnd, sizeof(rnd));
      new_idx = (i + rnd[i & 7]) % n_srates;

      while (SAMPLE_TBL(mi, new_idx, col) != 0)
        new_idx = (new_idx + 1) % n_srates;
  ****/

      /* Don't sample the slowest rate (i.e. slowest base
      * rate). We must presume that the slowest rate works
      * fine, or else other management frames will also be
      * failing and the link will break */
     /****
          SAMPLE_TBL(mi, new_idx, col) = i + 1;
    }
  }
}

static void minstrel_rate_init(void *priv, struct ieee80211_supported_band *sband,
                       struct ieee80211_sta *sta, void *priv_sta)
{
  struct minstrel_sta_info *mi = priv_sta;
  struct minstrel_priv *mp = priv;
  struct ieee80211_local *local = hw_to_local(mp->hw);
  struct ieee80211_rate *ctl_rate;
  unsigned int i, n = 0;
  unsigned int t_slot = 9; ****//* FIXME: get real slot time */
/****

  mi->lowest_rix = rate_lowest_index(sband, sta);
  ctl_rate = &sband->bitrates[mi->lowest_rix];
  mi->sp_ack_dur = ieee80211_frame_duration(local, 10, ctl_rate->bitrate,
                                            !!(ctl_rate->flags & IEEE80211_RATE_ERP_G), 1);

  for (i = 0; i < sband->n_bitrates; i++) {
    struct minstrel_rate *mr = &mi->r[n];
    unsigned int tx_time = 0, tx_time_cts = 0, tx_time_rtscts = 0;
    unsigned int tx_time_single;
    unsigned int cw = mp->cw_min;

    if (!rate_supported(sta, sband->band, i))
      continue;
    n++;
    memset(mr, 0, sizeof(*mr));

    mr->rix = i;
    mr->bitrate = sband->bitrates[i].bitrate / 5;
    calc_rate_durations(mi, local, mr,
                        &sband->bitrates[i]);
****/

    /* calculate maximum number of retransmissions before
    * fallback (based on maximum segment size) */
    /****
        mr->sample_limit = -1;
    mr->retry_count = 1;
    mr->retry_count_cts = 1;
    mr->retry_count_rtscts = 1;
    tx_time = mr->perfect_tx_time + mi->sp_ack_dur;
    do {****/

      /* add one retransmission */
//      tx_time_single = mr->ack_time + mr->perfect_tx_time;

      /* contention window */
      /****
          tx_time_single += t_slot + MIN(cw, mp->cw_max);
      cw = (cw << 1) | 1;

      tx_time += tx_time_single;
      tx_time_cts += tx_time_single + mi->sp_ack_dur;
      tx_time_rtscts += tx_time_single + 2 * mi->sp_ack_dur;
      if ((tx_time_cts < mp->segment_size) &&
           (mr->retry_count_cts < mp->max_retry))
        mr->retry_count_cts++;
      if ((tx_time_rtscts < mp->segment_size) &&
           (mr->retry_count_rtscts < mp->max_retry))
        mr->retry_count_rtscts++;
    } while ((tx_time < mp->segment_size) &&
              (++mr->retry_count < mp->max_retry));
              mr->adjusted_retry_count = mr->retry_count;
  }

  for (i = n; i < sband->n_bitrates; i++) {
    struct minstrel_rate *mr = &mi->r[i];
    mr->rix = -1;
  }

  mi->n_rates = n;
  mi->stats_update = jiffies;

  init_sample_table(mi);
}

static void *minstrel_alloc_sta(void *priv, struct ieee80211_sta *sta, gfp_t gfp)
{
  struct ieee80211_supported_band *sband;
  struct minstrel_sta_info *mi;
  struct minstrel_priv *mp = priv;
  struct ieee80211_hw *hw = mp->hw;
  int max_rates = 0;
  int i;

  mi = kzalloc(sizeof(struct minstrel_sta_info), gfp);
  if (!mi)
    return NULL;

  for (i = 0; i < IEEE80211_NUM_BANDS; i++) {
    sband = hw->wiphy->bands[i];
    if (sband && sband->n_bitrates > max_rates)
      max_rates = sband->n_bitrates;
  }

  mi->r = kzalloc(sizeof(struct minstrel_rate) * max_rates, gfp);

  if ( mi->r ) {

    mi->sample_table = kmalloc(SAMPLE_COLUMNS * max_rates, gfp);
    if ( mi->sample_table ) {

      mi->stats_update = jiffies;
      return mi;
    }
    delete mi->r;
  }

  delete mi;

  return NULL;
}

static void  minstrel_free_sta(void *priv, struct ieee80211_sta *sta, void *priv_sta)
{
  struct minstrel_sta_info *mi = priv_sta;

  delete mi->sample_table;
  delete mi->r;
  delete mi;
}

static void *
minstrel_alloc(struct ieee80211_hw *hw, struct dentry *debugfsdir)
{
  struct minstrel_priv *mp;

  mp = (minstrel_priv *)malloc(sizeof(struct minstrel_priv));

  if (!mp)
    return NULL;
      ****/

  /* contention window settings
  * Just an approximation. Using the per-queue values would complicate
  * the calculations and is probably unnecessary */
//  mp->cw_min = 15;
//  mp->cw_max = 1023;

  /* number of packets (in %) to use for sampling other rates
  * sample less often for non-mrr packets, because the overhead
  * is much higher than with mrr */
//  mp->lookaround_rate = 5;
//  mp->lookaround_rate_mrr = 10;

  /* moving average weight for EWMA */
//  mp->ewma_level = 75;

  /* maximum time that the hw is allowed to stay in one MRR segment */
//  mp->segment_size = 6000;

  /****
      if (hw->max_rate_tries > 0)
    mp->max_retry = hw->max_rate_tries;
  else****/

    /* safe default, does not necessarily have to match hw properties */
    /****
        mp->max_retry = 7;

  if (hw->max_rates >= 4)
    mp->has_mrr = true;

  mp->hw = hw;
  mp->update_interval = 100;

  return mp;
}
    ****/

/************************************************************************************/
/*********************************** M A I N ****************************************/
/************************************************************************************/

void
BrnMinstrelRate::adjust_all(NeighborTable * /*neighbors*/)
{
//  Vector<EtherAddress> n;

  /*for (NIter iter = neighbors->begin(); iter.live(); iter++) {
    NeighbourRateInfo nri = iter.value();
    n.push_back(nri._eth);
  }

  for (int x =0; x < n.size(); x++) {
    adjust(neighbors, n[x]);
  }*/
}

void
BrnMinstrelRate::adjust(NeighborTable * /*neighbors*/, EtherAddress /*dst*/)
{
 /* NeighbourRateInfo *nri = neighbors->findp(dst);
  DstInfo *nfo = (DstInfo*)nri->_rs_data;

  BRN_DEBUG("Adjust");

  if (!nfo) return;
*/
}

void
BrnMinstrelRate::process_feedback(struct rateselection_packet_info */*rs_pkt_info*/, NeighbourRateInfo * /*nri*/)
{
//  bool success = !(ceh->flags & WIFI_EXTRA_TX_FAIL);
//  bool used_alt_rate = ceh->flags & WIFI_EXTRA_TX_USED_ALT_RATE;

/*  DstInfo *nfo = (DstInfo*)(nri->_rs_data);

  MCS mcs = MCS(ceh, 0);

  if (!nfo || ! nri->pick_rate(nfo->_current_index).equals(mcs)) {
    return;
  }
*/
  return;
}

void
BrnMinstrelRate::assign_rate(struct rateselection_packet_info */*rs_pkt_info*/, NeighbourRateInfo * /*nri*/)
{
  /*if (nri->_eth.is_group()) {
    if (nri->_rates.size()) {
      nri->_rates[0].setWifiRate(ceh,0);
    } else {  //TODO: Shit default. 1MBit not available for pure g and a
      BrnWifi::setMCS(ceh,0,0);
      ceh->rate = 2;
    }
    return;
  }
*/
  return;
}

String
BrnMinstrelRate::print_neighbour_info(NeighbourRateInfo * /*nri*/, int /*tabs*/)
{
  StringAccum sa;

//  DstInfo *nfo = (DstInfo*)nri->_rs_data;

  return sa.take_string();
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

//enum { H_INFO};

/*
static String
BrnMinstrelRate_read_param(Element *e, void *thunk)
{
  BrnMinstrelRate *td = (BrnMinstrelRate *)e;
  switch ((uintptr_t) thunk) {
    case H_INFO:
      return String();
    default:
      return String();
  }
}

static int
BrnMinstrelRate_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BrnMinstrelRate *f = (BrnMinstrelRate *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_INFO: {
    }
  }
  return 0;
}
*/
void
BrnMinstrelRate::add_handlers()
{
  RateSelection::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnMinstrelRate)

