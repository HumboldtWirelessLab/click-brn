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

    void assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    void process_feedback(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int _stepup;
    int _stepdown;

    struct DstInfo {
      public:
        int _current_index;
        int _successes;

        int _stepup;
        bool _wentup;

        DstInfo(): _current_index(0), _successes(0), _stepup(0), _wentup(false) {
        }
    };

    bool _alt_rate;
    bool _adaptive_stepup;

    MCS mcs_zero;
};

CLICK_ENDDECLS
#endif
