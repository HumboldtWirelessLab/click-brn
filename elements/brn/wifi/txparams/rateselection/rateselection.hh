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
#define RATESELECTION_ROUNDROBIN        6 /* */


#define RATESELECTION_ADJUST_PERIOD_NONE             0
#define RATESELECTION_ADJUST_PERIOD_ON_STATS_UPDATE -1

struct rateselection_packet_info {
  click_wifi_extra *ceh;
  struct brn_click_wifi_extra_extention *wee;
  Packet *p;
  bool has_wifi_header;
};

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

    virtual void assign_rate(struct rateselection_packet_info *, NeighbourRateInfo *) = 0;
    virtual void process_feedback(struct rateselection_packet_info *, NeighbourRateInfo *) = 0;

    virtual int get_adjust_period() { return RATESELECTION_ADJUST_PERIOD_NONE;}
    virtual void adjust_all(NeighborTable *) {};

    virtual void init(BrnAvailableRates *) {};

    virtual String print_neighbour_info(NeighbourRateInfo *, int tabs = 0) = 0;

    void process_foreign(struct rateselection_packet_info *, NeighbourRateInfo *) {}

    void sort_rates_by_data_rate(NeighbourRateInfo *);
};

CLICK_ENDDECLS
#endif
