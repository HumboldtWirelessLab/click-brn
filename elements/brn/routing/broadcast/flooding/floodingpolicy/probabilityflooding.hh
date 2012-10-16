#ifndef PROBABILITYFLOODING_HH
#define PROBABILITYFLOODING_HH
#include <click/timer.hh>

#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
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
    bool do_forward(EtherAddress *src, EtherAddress *fwd, const EtherAddress *rcv, uint32_t id, bool is_known);
    void add_broadcast(EtherAddress *, uint32_t ) {};
    int policy_id();

    String flooding_info(void);

  private:

    BRN2NodeIdentity *_me;
    Brn2LinkTable *_link_table;
    int _max_metric_to_neighbor;

    void get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out);

};

CLICK_ENDDECLS
#endif
