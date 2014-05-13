#ifndef BO_FLOODING_HH
#define BO_FLOODING_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>

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
  static const uint16_t _bo_start  = 63; // initial backoff

};



CLICK_ENDDECLS

#endif /* BO_MAX_THROUGHPUT_HH */
