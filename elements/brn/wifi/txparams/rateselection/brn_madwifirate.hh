#ifndef BRNMADWIFIRATE_HH
#define BRNMADWIFIRATE_HH
#include <click/timer.hh>

#include "rateselection.hh"

CLICK_DECLS

class BrnMadwifiRate : public RateSelection
{
  public:
    BrnMadwifiRate();
    ~BrnMadwifiRate();

/*ELEMENT*/
    const char *class_name() const  { return "BrnMadwifiRate"; }
    const char *name() const { return "BrnMadwifiRate"; }
    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    void add_handlers();

    void adjust_all(NeighborTable *nt);
    void adjust(NeighborTable *nt, EtherAddress);

    void assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    void process_feedback(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int get_adjust_period() { return _period; }

    struct DstInfo {
      public:
        int _credits;

        int _current_index;

        int _successes;
        int _failures;
        int _retries;
    };

    int _stepup;
    int _stepdown;

    bool _alt_rate;
    int _period;

    MCS mcs_zero;
};

CLICK_ENDDECLS
#endif
