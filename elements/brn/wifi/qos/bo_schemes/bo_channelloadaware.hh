#ifndef BO_CHANNELLOADAWARE_HH
#define BO_CHANNELLOADAWARE_HH

#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/rxinfo/channelstats/channelstats.hh"
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
  bool handle_strategy(uint32_t strategy);
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
  static const uint16_t _bo_start = 32; // initial backoff
  static const uint8_t _tdiff_param = 3;  // target diff strategy wiggle room

  static const uint16_t _cla_min_cwmin = 16;
  static const uint16_t _cla_max_cwmin = 1024;

  ChannelStats *_cst;

  uint32_t _current_bo;

  /* busy aware */
  uint32_t _target_busy;

  /* target diff strategy */
  uint32_t _target_diff;
  uint32_t _last_diff;

  /* activate bo cap at lower/upper bound */
  uint8_t _cap;

  /* activate ignoring channel stats with the same id */
  uint8_t _cst_sync;
  uint32_t _last_id;
};



CLICK_ENDDECLS

#endif /* BO_CHANNELLOADAWARE_HH */
