#ifndef MULTIRATEFLOODING_HH
#define MULTIRATEFLOODING_HH
#include <click/timer.hh>

#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "floodingpolicy.hh"

CLICK_DECLS

class MultirateFlooding : public FloodingPolicy
{

  public:
    MultirateFlooding();
    ~MultirateFlooding();

/*ELEMENT*/
    const char *class_name() const  { return "MultirateFlooding"; }

    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    const char *floodingpolicy_name() const { return "MultirateFlooding"; }
    int floodingpolicy_id() const { return POLICY_ID_MULTIRATE; }

    bool do_forward(EtherAddress *src, EtherAddress *fwd, const EtherAddress *rcv, uint32_t id, bool is_known, uint32_t forward_count,
                    uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                    Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack);
    void init_broadcast(EtherAddress *, uint32_t, uint32_t *, uint8_t *,
                        Vector<EtherAddress> *, Vector<EtherAddress> *) {};
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
