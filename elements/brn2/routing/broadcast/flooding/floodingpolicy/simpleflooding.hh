#ifndef SIMPLEFLOODING_HH
#define SIMPLEFLOODING_HH
#include <click/timer.hh>

#include "floodingpolicy.hh"

CLICK_DECLS

class SimpleFlooding : public FloodingPolicy
{

  public:
    SimpleFlooding();
    ~SimpleFlooding();

/*ELEMENT*/
    const char *class_name() const  { return "SimpleFlooding"; }

    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    const char *floodingpolicy_name() const { return "SimpleFlooding"; }
    bool do_forward(EtherAddress *src, uint32_t id, bool is_known);
    void add_broadcast(EtherAddress *, uint32_t ) {};
    int policy_id();

    String flooding_info(void);

  private:

    int _debug;

};

CLICK_ENDDECLS
#endif
