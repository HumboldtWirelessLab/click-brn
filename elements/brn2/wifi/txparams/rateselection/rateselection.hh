#ifndef RATESELECTION_HH
#define RATESELECTION_HH
#include <click/element.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class RateSelection : public BRNElement
{
  public:
    RateSelection();
    ~RateSelection();

    void add_handlers();

    virtual const char *name() const = 0;

//    virtual void assign_rate(SetTXPowerRate::DstInfo *dst, struct click_wifi_extra *ceh);
//    virtual void process_feedback(EtherAddress *dst, struct click_wifi_extra *ceh);

};

CLICK_ENDDECLS
#endif
