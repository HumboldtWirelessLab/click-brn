#ifndef FLOODING_POLICY_HH
#define FLOODING_POLICY_HH
#include <click/element.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

#define POLICY_ID_SIMPLE      1
#define POLICY_ID_PROBABILITY 2
#define POLICY_ID_MPR         3

class FloodingPolicy : public BRNElement
{
  public:
    FloodingPolicy();
    ~FloodingPolicy();

    virtual const char *floodingpolicy_name() const = 0;
    virtual bool do_forward(EtherAddress *src, EtherAddress *fwd, EtherAddress *rcv, uint32_t id, bool is_known) = 0;
    virtual void add_broadcast(EtherAddress *src, uint32_t id) = 0; //used for local generated broadcast
    virtual int policy_id() = 0;

};

CLICK_ENDDECLS
#endif
