#ifndef BO_CHANNELLOADAWARE_HH
#define BO_CHANNELLOADAWARE_HH

#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/channelstats.hh"
#include "backoff_scheme.hh"


CLICK_DECLS



class BoChannelLoadAware : public BackoffScheme {
/* Derived Functions */
public:
  /* Element */
  const char *class_name() const  { return "BoChannelLoadAware"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  /* BackoffScheme */
  bool handle_strategy(uint32_t strategy) { return strategy == BACKOFF_STRATEGY_CHANNEL_LOAD_AWARE; }
  int get_cwmin(Packet *p, uint8_t tos);
  void handle_feedback(uint8_t retries);
  void set_conf(uint32_t min_cwmin, uint32_t max_cwmin);

public:
  BoChannelLoadAware();
  ~BoChannelLoadAware();

private:
  void increase_cw();
  void decrease_cw();


private:
  static const uint16_t _id               = 3;  // unique bo scheme identifier
  static const uint16_t _bo_start         = 63; // initial backoff

  static const uint8_t _target_load_param = 5;  // target load wiggle room

  static const uint16_t _tcl_min_cwmin    = 31;
  static const uint16_t _tcl_max_cwmin    = 1023;

  ChannelStats *_cst;

  uint32_t _target_channelload;
  uint32_t _bo_for_target_channelload;

  /* target channel load low and high water marks */
  uint32_t _tcl_lwm;
  uint32_t _tcl_hwm;

  /* activate bo cap at lower/upper bound */
  uint8_t _cap;
};



CLICK_ENDDECLS

#endif /* BO_CHANNELLOADAWARE_HH */
