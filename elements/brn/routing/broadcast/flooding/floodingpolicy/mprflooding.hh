#ifndef MPRFLOODING_HH
#define MPRFLOODING_HH
#include <click/timer.hh>

#include "../flooding_helper.hh"
#include "../flooding_db.hh"

#include "floodingpolicy.hh"

CLICK_DECLS

#define MPR_DEFAULT_UPDATE_INTERVAL 5000
#define MPR_DEFAULT_NB_METRIC         -1  /* Use metric of floodinghelper */

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
    int floodingpolicy_id() const { return POLICY_ID_MPR; };

    void set_mpr_header(uint32_t *tx_data_size, uint8_t *txdata, EtherAddress *src, int bcast_id);

    bool do_forward(EtherAddress *src, EtherAddress *fwd, const EtherAddress *rcv, uint32_t id, bool is_known, uint32_t forward_count,
                    uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                    Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack);

    void init_broadcast(EtherAddress *, uint32_t, uint32_t *, uint8_t *, Vector<EtherAddress> *, Vector<EtherAddress> *);

    int policy_id();

    String flooding_info(void);

    void set_mpr(HashMap<EtherAddress,EtherAddress> *known_nodes);

    void remove_finished_neighbours(Vector<EtherAddress> *neighbors, HashMap<EtherAddress, EtherAddress> *known_nodes);

    int set_mpr_vector(const String &in_s, Vector<EtherAddress> *ea_vector);
    int set_mpr_forwarder(const String &in_s);
    int set_mpr_destination(const String &in_s);

  private:

    BRN2NodeIdentity *_me;
    FloodingHelper *_fhelper;
    FloodingDB *_flooding_db;

    int _max_metric_to_neighbor;

    Timestamp _last_set_mpr_call;
    uint32_t _update_interval;
    bool _fix_mpr;

    Vector<EtherAddress> _mpr_forwarder;
    Vector<EtherAddress> _neighbours;
};

CLICK_ENDDECLS
#endif
