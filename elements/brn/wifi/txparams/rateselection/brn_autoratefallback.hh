#ifndef BRNAUTORATEFALLBACK_HH
#define BRNAUTORATEFALLBACK_HH
#include <click/timer.hh>

#include "rateselection.hh"

CLICK_DECLS

class BrnAutoRateFallback : public RateSelection
{

  public:
    BrnAutoRateFallback();
    ~BrnAutoRateFallback();

/*ELEMENT*/
    const char *class_name() const  { return "BrnAutoRateFallback"; }
    const char *name() const { return "BrnAutoRateFallback"; }

    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    void add_handlers();

    void assign_rate(click_wifi_extra *, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *);

    void process_feedback(click_wifi_extra *, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *);

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int _stepup;
    int _stepdown;

    struct DstInfo {
      public:
        int _current_index;
        int _successes;

        int _stepup;
        bool _wentup;

        DstInfo() {
        }
    };

    bool _alt_rate;
    bool _adaptive_stepup;

    MCS mcs_zero;
};

CLICK_ENDDECLS
#endif
