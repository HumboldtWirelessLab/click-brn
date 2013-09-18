#ifndef MSTFLOODING_HH
#define MSTFLOODING_HH
#include <click/timer.hh>

#include <elements/brn/routing/identity/brn2_nodeidentity.hh>
#include "floodingpolicy.hh"

CLICK_DECLS

class MSTFlooding : public FloodingPolicy
{

  public:
    MSTFlooding();
    ~MSTFlooding();

/*ELEMENT*/
    const char *class_name() const  { return "MSTFlooding"; }

    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    const char *floodingpolicy_name() const { return "MSTFlooding"; }
    bool do_forward(EtherAddress *src, EtherAddress *fwd, const EtherAddress *rcv, uint32_t id, bool is_known, uint32_t forward_count,
                    uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                    Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack);
    void init_broadcast(EtherAddress *, uint32_t, uint32_t *, uint8_t *,
                        Vector<EtherAddress> *, Vector<EtherAddress> *) {};
    int policy_id();

    String flooding_info(void);

  private:
    Vector<int> followers;
    String _circle_path;
    void get_neighbours(String path);
    BRN2NodeIdentity *_me;

};

CLICK_ENDDECLS
#endif
