#ifndef RATESELECTION_HH
#define RATESELECTION_HH
#include <click/element.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/wifi/txparams/neighbourrateinfo.hh"

CLICK_DECLS

class RateSelection : public BRNElement
{
  public:
    RateSelection();
    ~RateSelection();

    void add_handlers();

    virtual const char *name() const = 0;

    virtual void assign_rate(click_wifi_extra *, NeighbourRateInfo *) = 0;
    virtual void process_feedback(click_wifi_extra *, NeighbourRateInfo *) = 0;

    virtual int get_adjust_period() { return 0;}
    virtual void adjust_all(NeighborTable *) {};

    virtual String print_neighbour_info(NeighbourRateInfo *, int tabs = 0) = 0;

    void process_foreign(click_wifi_extra *, NeighbourRateInfo *) {}

    void sort_rates_by_data_rate(NeighbourRateInfo *);

};

CLICK_ENDDECLS
#endif
