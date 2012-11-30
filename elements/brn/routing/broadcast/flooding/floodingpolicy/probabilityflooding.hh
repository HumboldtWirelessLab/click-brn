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
    bool do_forward(EtherAddress *src, EtherAddress *fwd, const EtherAddress *rcv, uint32_t id, bool is_known,
                    uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                    Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack);
    void init_broadcast(EtherAddress *, uint32_t, uint32_t *, uint8_t *,
                        Vector<EtherAddress> *, Vector<EtherAddress> *) {};
    int policy_id();

    String flooding_info(void);

  private:

    BRN2NodeIdentity *_me;
    Brn2LinkTable *_link_table;

    uint32_t _min_no_neighbors;
    uint32_t _fwd_probability;
    int _max_metric_to_neighbor;

    void get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out);

};

CLICK_ENDDECLS
#endif
