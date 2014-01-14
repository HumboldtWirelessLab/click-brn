#ifndef BO_MAX_THROUGHPUT_HH
#define BO_MAX_THROUGHPUT_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>
#include <elements/brn/wifi/rxinfo/channelstats.hh>

#include "backoff_scheme.hh"

CLICK_DECLS



class BoMaxThroughput : public BackoffScheme {
/* Derived Functions */
public:
  /* Element */
  const char *class_name() const  { return "BoMaxThroughput"; }
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
  BoMaxThroughput();
  ~BoMaxThroughput();

private:
  void set_strategy(uint32_t strategy);


private:
  static const uint16_t _bo_start  = 63; // initial backoff

  ChannelStats *_cst;
  uint32_t _strategy;
};



CLICK_ENDDECLS

#endif /* BO_MAX_THROUGHPUT_HH */
