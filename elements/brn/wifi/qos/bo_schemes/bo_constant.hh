#ifndef BO_CONSTANT_HH
#define BO_CONSTANT_HH

#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/rxinfo/channelstats/channelstats.hh"
#include "backoff_scheme.hh"


CLICK_DECLS



class BoConstant : public BackoffScheme {
/* Derived Functions */
public:
  /* Element */
  const char *class_name() const  { return "BoConstant"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  /* BackoffScheme */
  int get_cwmin(Packet *p, uint8_t tos);
  void handle_feedback(uint8_t retries);
  void set_conf(uint32_t min_cwmin, uint32_t max_cwmin);


public:
  BoConstant();
  ~BoConstant();

private:

  void increase_cw();
  void decrease_cw();


private:
  static const uint16_t _id               = 8;  // unique bo scheme identifier

  ChannelStats *_cst;

  uint16_t _const_bo;
};



CLICK_ENDDECLS

#endif /* BO_CHANNELLOADAWARE_HH */
