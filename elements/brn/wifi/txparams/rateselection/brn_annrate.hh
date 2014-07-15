#ifndef BRNANNRATE_HH
#define BRNANNRATE_HH
#include <click/timer.hh>

#include "rateselection.hh"
#include "brn_annrate_net.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/wifi/rxinfo/hiddennodedetection.hh"

CLICK_DECLS


class BrnAnnRate : public RateSelection
{
  public:

	BRN2LinkStat *_linkstat;
	HiddenNodeDetection *_hnd;
	BrnAnnRateNet *_ann;


    BrnAnnRate();
    ~BrnAnnRate();

/*ELEMENT*/
    const char *class_name() const  { return "BrnAnnRate"; }
    const char *name() const { return "BrnAnnRate"; }
    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    void adjust_all(NeighborTable *nt);

    void add_handlers();

    void assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    void process_feedback(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int get_adjust_period() { return RATESELECTION_ADJUST_PERIOD_ON_STATS_UPDATE; }
    
    int rate_to_index(uint8_t rate_mbits):

};

CLICK_ENDDECLS
#endif