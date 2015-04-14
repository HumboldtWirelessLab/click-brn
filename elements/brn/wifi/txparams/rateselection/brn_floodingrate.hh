#ifndef BRNFLOODINGRATE_HH
#define BRNFLOODINGRATE_HH
#include <click/timer.hh>

#include "rateselection.hh"

#include "elements/brn/routing/broadcast/flooding/flooding.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_db.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_helper.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/wifi/rxinfo/channelstats/channelstats.hh"

CLICK_DECLS

#define FLOODINGRATE_DEFAULT_VALUES          0
#define FLOODINGRATE_SINGLE_MAXRATE          1
#define FLOODINGRATE_SINGLE_MINPOWER         2
#define FLOODINGRATE_SINGLE_BEST_POWER_RATE  3
#define FLOODINGRATE_GROUP_MAXRATE           4
#define FLOODINGRATE_GROUP_MINPOWER          5
#define FLOODINGRATE_GROUP_BEST_POWER_RATE   6

class BrnFloodingRate : public RateSelection
{
  public:
    typedef HashMap<BrnRateSize, int> BrnRateSize2EffectiveRate;
    typedef BrnRateSize2EffectiveRate::const_iterator BrnRateSize2EffectiveRateIter;

    typedef HashMap<BrnRateSize, int> BrnRateSize2RSSI;
    typedef BrnRateSize2RSSI::const_iterator BrnRateSize2RSSIIter;

    typedef HashMap<BrnRateSize, int> BrnRateSize2Count;

    BrnFloodingRate();
    ~BrnFloodingRate();

/*ELEMENT*/
    const char *class_name() const  { return "BrnFloodingRate"; }
    const char *name() const { return "BrnFloodingRate"; }
    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    void add_handlers();

    bool controls_power() { return true; }

    void assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    void process_feedback(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int get_adjust_period() { return RATESELECTION_ADJUST_PERIOD_NONE; }

    /** internal functions **/
    int get_best_rate(EtherAddress &ether, MCS *best_rate);
    int get_min_power(EtherAddress &ether);
    int get_best_rate_min_power(EtherAddress &ether, MCS *best_rate);

    int get_best_rate_group(Vector<EtherAddress> &group, MCS *best_rate);
    int get_min_power_group(Vector<EtherAddress> &group);
    int get_best_rate_min_power_group(Vector<EtherAddress> &group, MCS *best_rate);

    int get_group_info(int mode, Vector<EtherAddress> &group, MCS *best_rate);

    int metric_space_bits_per_second(MCS &rate, uint32_t tx_power);
    void get_group(Vector<EtherAddress> *group);

    String print_stats(int /*tabs*/);

    Flooding *_flooding;
    FloodingHelper *_fhelper;
    FloodingDB *_flooding_db;
    BRN2LinkStat *_linkstat;
    ChannelStats *_cst;

    uint32_t _fl_rate_strategy;

    MCS mcs_zero;

    uint32_t _dflt_retries;

    /* stats */
    uint32_t _no_pkts;
    uint32_t _saved_power_sum;
};

CLICK_ENDDECLS
#endif
