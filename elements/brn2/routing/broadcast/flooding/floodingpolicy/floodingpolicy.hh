#ifndef FLOODING_POLICY_HH
#define FLOODING_POLICY_HH
#include <click/element.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

#define POLICY_ID_SIMPLE      1
#define POLICY_ID_PROBABILITY 2

class FloodingPolicy : public BRNElement
{
  public:
    FloodingPolicy();
    ~FloodingPolicy();

    virtual const char *floodingpolicy_name() const = 0;
    virtual bool do_forward(EtherAddress *src, int id, bool is_known) = 0;
    virtual void add_broadcast(EtherAddress *src, int id) = 0;
    virtual int policy_id() = 0;

};

CLICK_ENDDECLS
#endif
