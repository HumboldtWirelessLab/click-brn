#ifndef BO_MAX_THROUGHPUT_HH
#define BO_MAX_THROUGHPUT_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>
#include <elements/brn/wifi/rxinfo/channelstats/channelstats.hh>

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

  /* BackoffScheme */
  int get_cwmin(Packet *p, uint8_t tos);

public:
  BoMaxThroughput();
  ~BoMaxThroughput();

private:

  ChannelStats *_cst;
  int32_t _backoff_offset;
};

CLICK_ENDDECLS

#endif /* BO_MAX_THROUGHPUT_HH */
