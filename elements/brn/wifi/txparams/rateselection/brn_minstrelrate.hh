#ifndef BRNMINSTRELRATE_HH
#define BRNMINSTRELRATE_HH
#include <click/timer.hh>

#include "rateselection.hh"

CLICK_DECLS

class BrnMinstrelRate : public RateSelection
{
  public:
    BrnMinstrelRate();
    ~BrnMinstrelRate();

/*ELEMENT*/
    const char *class_name() const  { return "BrnMinstrelRate"; }
    const char *name() const { return "BrnMinstrelRate"; }
    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    void adjust_all(NeighborTable *nt);
    void adjust(NeighborTable *nt, EtherAddress);

    void add_handlers();

    void assign_rate(click_wifi_extra *, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *);

    void process_feedback(click_wifi_extra *, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *);

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int get_adjust_period() { return _period; }

    int _period;

/* Source: rc80211_minstrel.h of mac80211 (linux-wireless) */

    struct minstrel_rate {
      int32_t bitrate;
      int32_t rix;

      uint32_t perfect_tx_time;
      uint32_t ack_time;

      int32_t sample_limit;
      uint32_t retry_count;
      uint32_t retry_count_cts;
      uint32_t retry_count_rtscts;
      uint32_t adjusted_retry_count;

      uint32_t success;
      uint32_t attempts;
      uint32_t last_attempts;
      uint32_t last_success;

      /* parts per thousand */
      uint32_t cur_prob;
      uint32_t probability;

      /* per-rate throughput */
      uint32_t cur_tp;

      uint64_t succ_hist;
      uint64_t att_hist;
    };

    struct minstrel_sta_info {
      Timestamp stats_update;
      uint32_t sp_ack_dur;
      uint32_t rate_avg;

      uint32_t lowest_rix;

      uint32_t max_tp_rate;
      uint32_t max_tp_rate2;
      uint32_t max_prob_rate;
      uint32_t packet_count;
      uint32_t sample_count;
      int32_t sample_deferred;

      uint32_t sample_idx;
      uint32_t sample_column;

      int32_t n_rates;
      struct minstrel_rate *r;
      bool prev_sample;

      /* sampling table */
      uint8_t *sample_table;
    };

    struct minstrel_priv {
      struct ieee80211_hw *hw;
      bool has_mrr;
      uint32_t cw_min;
      uint32_t cw_max;
      uint32_t max_retry;
      uint32_t ewma_level;
      uint32_t segment_size;
      uint32_t update_interval;
      uint32_t lookaround_rate;
      uint32_t lookaround_rate_mrr;
    };

    MCS mcs_zero;
};

CLICK_ENDDECLS
#endif
