#ifndef RATESELECTION_HH
#define RATESELECTION_HH
#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/schemelist.hh"
#include "elements/brn/wifi/txparams/neighbourrateinfo.hh"

CLICK_DECLS

#define RATESELECTION_NONE              0 /* default */
#define RATESELECTION_FIXRATE           1 /* */
#define RATESELECTION_MINSTREL          2 /* */
#define RATESELECTION_MADWIFI           3 /* */
#define RATESELECTION_AUTORATEFALLBACK  4 /* */
#define RATESELECTION_FLOODING          5 /* */

class RateSelection : public BRNElement, public Scheme
{
  public:
    RateSelection();
    ~RateSelection();

    void add_handlers();

    void *cast(const char *name);

    virtual const char *name() const = 0;

    virtual bool handle_strategy(uint32_t strategy);
    virtual uint32_t get_strategy();
    virtual void set_strategy(uint32_t strategy);

    virtual void assign_rate(click_wifi_extra *, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *) = 0;
    virtual void process_feedback(click_wifi_extra *, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *) = 0;

    virtual int get_adjust_period() { return 0;}
    virtual void adjust_all(NeighborTable *) {};

    virtual void init(BrnAvailableRates *) {};

    virtual String print_neighbour_info(NeighbourRateInfo *, int tabs = 0) = 0;

    void process_foreign(click_wifi_extra *, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *) {}

    void sort_rates_by_data_rate(NeighbourRateInfo *);
};

CLICK_ENDDECLS
#endif
