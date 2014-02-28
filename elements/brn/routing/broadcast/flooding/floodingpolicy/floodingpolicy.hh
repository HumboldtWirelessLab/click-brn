#ifndef FLOODING_POLICY_HH
#define FLOODING_POLICY_HH
#include <click/element.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

#define POLICY_ID_SIMPLE      1
#define POLICY_ID_PROBABILITY 2
#define POLICY_ID_MPR         3
#define POLICY_ID_OVERLAY     4

#define POLICY_ID_MST         5


/** TODO: add extra element for FloodingInfd (like db)
 */
class Flooding;

struct flooding_policy_rx_info {
   uint32_t rx_data_size;
   uint8_t *rxdata;

   bool is_known;
   uint32_t forward_count;
};

struct flooding_policy_tx_info {
  uint32_t *tx_data_size;
  uint8_t *txdata;

  Vector<EtherAddress> *unicast_dst; //which nodes must be target for unicast
  Vector<EtherAddress> *passiveack;  //which nodes should have the msg
  Vector<int> *net_retry_timing;
};

class FloodingPolicy : public BRNElement
{
  public:
    FloodingPolicy();
    ~FloodingPolicy();

    virtual const char *floodingpolicy_name() const = 0;
    virtual int floodingpolicy_id() const = 0;

    virtual void init_broadcast(EtherAddress *src, uint32_t id, uint32_t *tx_data_size, uint8_t *txdata,
                                 Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack ) = 0; //used for local generated broadcast

    virtual bool do_forward(EtherAddress *src, EtherAddress *fwd, const EtherAddress *rcv, uint32_t id, bool is_known, uint32_t forward_count,
                             uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                             Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack) = 0;

    /** update_info or on_finshed */
    /*virtual bool update_fwd(EtherAddress *src, EtherAddress *fwd, const EtherAddress *rcv, uint32_t id, bool is_known, uint32_t forward_count,
                             uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                             Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack) = 0;
    */
    virtual int policy_id() = 0;


    Flooding *_flooding;
    void set_flooding(Flooding *fl) { _flooding = fl; }
};

CLICK_ENDDECLS
#endif
