#ifndef BO_NEIGHBOURS_HH
#define BO_NEIGHBOURS_HH

#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/rxinfo/channelstats.hh"
#include "backoff_scheme.hh"


CLICK_DECLS



class BoNeighbours : public BackoffScheme {
/* Derived Functions */
public:
  /* Element */
  const char *class_name() const  { return "BoNeighbours"; }
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
  BoNeighbours();
  ~BoNeighbours();

private:
  void set_strategy(uint32_t strategy);

private:
  static const uint32_t _bo_start = 31;
  static const uint8_t alpha = 85;
  static const uint8_t beta  = 50;

  ChannelStats *_cst;
  uint8_t _cst_sync;
  uint32_t _strategy;
  uint32_t _last_id;
  int32_t _current_bo;
};



CLICK_ENDDECLS

#endif /* BO_NEIGHBOURS_HH */
