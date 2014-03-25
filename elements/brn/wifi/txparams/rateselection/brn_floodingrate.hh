#ifndef BRNFLOODINGRATE_HH
#define BRNFLOODINGRATE_HH
#include <click/timer.hh>

#include "rateselection.hh"
#include "elements/brn/routing/broadcast/flooding/flooding.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_helper.hh"

CLICK_DECLS

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

    void adjust_all(NeighborTable *nt);
    void adjust(NeighborTable *nt, EtherAddress);

    void assign_rate(click_wifi_extra *, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *);

    void process_feedback(click_wifi_extra *, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *);

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int get_adjust_period() { return -1; }

    Flooding *_flooding;
    FloodingHelper *_fhelper;

    MCS mcs_zero;
};

CLICK_ENDDECLS
#endif
