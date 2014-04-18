#ifndef OVERLAYFLOODING_HH
#define OVERLAYFLOODING_HH
#include <click/timer.hh>

#include <elements/brn/routing/identity/brn2_nodeidentity.hh>
#include <elements/brn/topology/overlay/overlay_structure.hh>
#include "floodingpolicy.hh"

CLICK_DECLS

class OverlayFlooding : public FloodingPolicy
{

  public:
    OverlayFlooding();
    ~OverlayFlooding();

/*ELEMENT*/
    const char *class_name() const  { return "OverlayFlooding"; }

    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    const char *floodingpolicy_name() const { return "OverlayFlooding"; }
    int floodingpolicy_id() const { return POLICY_ID_OVERLAY; }

    bool do_forward(EtherAddress *src, EtherAddress *fwd, const EtherAddress *rcv, uint32_t id, bool is_known, uint32_t forward_count,
                    uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                    Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack);
    void init_broadcast(EtherAddress *, uint32_t, uint32_t *, uint8_t *,
                        Vector<EtherAddress> *, Vector<EtherAddress> *);
    int policy_id();

    String flooding_info(void);
    Vector<EtherAddress> followers;

  private:

    BRN2NodeIdentity *_me;
    OverlayStructure *_ovl;

};

CLICK_ENDDECLS
#endif
