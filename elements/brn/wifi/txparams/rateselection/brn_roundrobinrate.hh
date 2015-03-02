#ifndef BRNROUNDROBINRATE_HH
#define BRNROUNDROBINRATE_HH
#include <click/timer.hh>

#include "rateselection.hh"
#include "elements/brn/wifi/brnwifi.hh"

CLICK_DECLS

class BrnRoundRobinRate : public RateSelection
{
  public:
    BrnRoundRobinRate();
    ~BrnRoundRobinRate();

/*ELEMENT*/
    const char *class_name() const  { return "BrnRoundRobinRate"; }
    const char *name() const { return "BrnRoundRobinRate"; }
    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    void add_handlers();

    void assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);
    void process_feedback(struct rateselection_packet_info *, NeighbourRateInfo *) {};

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int get_adjust_period() { return RATESELECTION_ADJUST_PERIOD_NONE; }

    uint8_t _tries;

};

CLICK_ENDDECLS
#endif
