#ifndef POWERCONTROL_HH
#define POWERCONTROL_HH
#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/schemelist.hh"
#include "elements/brn/wifi/txparams/neighbourrateinfo.hh"
#include "elements/brn/wifi/txparams/rateselection/rateselection.hh"

CLICK_DECLS

#define POWERCONTROL_NONE              0 /* default */
#define POWERCONTROL_FIXPOWER          1 /* */
#define POWERCONTROL_ROUNDROBIN        2 /* */

class PowerControl : public BRNElement, public Scheme
{
  public:
    PowerControl();
    ~PowerControl();

    void add_handlers();

    void *cast(const char *name);

    virtual const char *name() const = 0;

    virtual bool handle_strategy(uint32_t strategy);
    virtual uint32_t get_strategy();
    virtual void set_strategy(uint32_t strategy);

    virtual void assign_power(struct rateselection_packet_info *, NeighbourRateInfo *) = 0;

    virtual void init(BrnAvailableRates *) {};
};

CLICK_ENDDECLS
#endif
