#ifndef BO_FLOODING_HH
#define BO_FLOODING_HH

#include "elements/brn/wifi/rxinfo/hiddennodedetection.hh"
#include "elements/brn/routing/broadcast/flooding/flooding.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_helper.hh"

#include "backoff_scheme.hh"

CLICK_DECLS

class BoFlooding : public BackoffScheme {
 /* Derived Functions */
 public:
  /* Element */
  const char *class_name() const  { return "BoFlooding"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  /* BackoffScheme */
  int get_cwmin(Packet *p, uint8_t tos);
  void handle_feedback(uint8_t retries);

 public:
  BoFlooding();
  ~BoFlooding();

 private:
  Flooding *_flooding;
  FloodingHelper *_fhelper;

};

CLICK_ENDDECLS

#endif /* BO_FLOODING_HH */
