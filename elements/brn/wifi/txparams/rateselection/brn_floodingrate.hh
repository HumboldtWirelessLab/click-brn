#ifndef BRNFLOODINGRATE_HH
#define BRNFLOODINGRATE_HH
#include <click/timer.hh>

#include "rateselection.hh"
#include "elements/brn/routing/broadcast/flooding/flooding.hh"
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

    void assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    void process_feedback(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int get_adjust_period() { return -1; }

    /** internal functions **/
    int get_best_rate(EtherAddress &ether, MCS *best_rate);
    int get_min_power(EtherAddress &ether);
    int get_best_rate_min_power(EtherAddress &ether, MCS *best_rate);

    int metric_space_bits_per_second(MCS &rate, uint32_t tx_power);

    String print_stats(int /*tabs*/);

    Flooding *_flooding;
    FloodingHelper *_fhelper;
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
