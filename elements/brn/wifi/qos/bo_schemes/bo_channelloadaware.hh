#ifndef BO_CHANNELLOADAWARE_HH
#define BO_CHANNELLOADAWARE_HH

#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/rxinfo/channelstats/channelstats.hh"
#include "backoff_scheme.hh"


CLICK_DECLS

#define BO_CHANNELLOADAWARE_START_BO 32

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

 public:
  BoChannelLoadAware();
  ~BoChannelLoadAware();

 private:

  void increase_cw();
  void decrease_cw();


private:

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
