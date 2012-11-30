#ifndef MPRFLOODING_HH
#define MPRFLOODING_HH
#include <click/timer.hh>

#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "floodingpolicy.hh"

CLICK_DECLS

#define MPR_DEFAULT_UPDATE_INTERVAL 5000

class MPRFlooding : public FloodingPolicy
{

  public:
    MPRFlooding();
    ~MPRFlooding();

/*ELEMENT*/
    const char *class_name() const  { return "MPRFlooding"; }

    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    const char *floodingpolicy_name() const { return "MPRFlooding"; }
    bool do_forward(EtherAddress *src, EtherAddress *fwd, const EtherAddress *rcv, uint32_t id, bool is_known,
                    uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                    Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack);
    void add_broadcast(EtherAddress *, uint32_t ) {};
    int policy_id();

    String flooding_info(void);

    void set_mpr();

    int set_mpr_vector(const String &in_s, Vector<EtherAddress> *ea_vector);
    int set_mpr_forwarder(const String &in_s);
    int set_mpr_destination(const String &in_s);

  private:

    BRN2NodeIdentity *_me;
    Brn2LinkTable *_link_table;
    int _max_metric_to_neighbor;

    Timestamp _last_set_mpr_call;
    uint32_t _update_interval;
    bool _fix_mpr;

    Vector<EtherAddress> _mpr_forwarder;
    Vector<EtherAddress> _mpr_unicast;
    Vector<EtherAddress> _neighbours;

    void get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out);

};

CLICK_ENDDECLS
#endif
