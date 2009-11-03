#ifndef PROBABILITYFLOODING_HH
#define PROBABILITYFLOODING_HH
#include <click/timer.hh>

#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"
#include "floodingpolicy.hh"

CLICK_DECLS

class ProbabilityFlooding : public FloodingPolicy
{

  public:
    ProbabilityFlooding();
    ~ProbabilityFlooding();

/*ELEMENT*/
    const char *class_name() const  { return "ProbabilityFlooding"; }

    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    const char *floodingpolicy_name() const { return "ProbabilityFlooding"; }
    bool do_forward(EtherAddress *src, int id, bool is_known);
    void add_broadcast(EtherAddress *, int ) {};
    int policy_id();

    String flooding_info(void);

  private:

    BRN2LinkStat *_linkstat;
    int _debug;

};

CLICK_ENDDECLS
#endif
