#ifndef BO_TARGETDIFF_RXTXBUSY
#define BO_TARGETDIFF_RXTXBUSY

#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/channelstats.hh"
#include "backoff_scheme.hh"

CLICK_DECLS



class BoTargetDiffRxTxBusy : public BackoffScheme {
/* Derived Functions */
public:
  /* Element */
  const char *class_name() const  { return "BoTargetDiffRxTxBusy"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  /* BackoffScheme */
  uint16_t get_id();
  int get_cwmin(Packet *p, uint8_t tos);
  void handle_feedback(uint8_t retries);
  void set_conf(uint32_t min_cwmin, uint32_t max_cwmin);


public:
  BoTargetDiffRxTxBusy();
  ~BoTargetDiffRxTxBusy();

private:
  void increase_cw();
  void decrease_cw();


private:
  static const uint16_t _id               = 6;  // unique bo scheme identifier
  static const uint16_t _bo_start         = 63; // initial backoff
  static const uint8_t _targetdiff_param  = 3;

  ChannelStats *_cst;
  uint32_t _targetdiff;
  uint32_t _bo_for_targetdiff;
  uint32_t _tdiff_lwm;
  uint32_t _tdiff_hwm;
};



CLICK_ENDDECLS

#endif /* BO_TARGETDIFF_RXTXBUSY */
