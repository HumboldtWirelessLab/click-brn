#ifndef BO_OFF_HH
#define BO_OFF_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>

#include "backoff_scheme.hh"

CLICK_DECLS


class BoOff : public BackoffScheme {

/* Derived Functions */
public:
  /* Element */
  const char *class_name() const  { return "BoOff"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  /* BackoffScheme */
  uint16_t get_id();
  int get_cwmin(Packet *p, uint8_t tos);
  void handle_feedback(uint8_t retries);


/* Own Functions */
public:
  BoOff();
  ~BoOff();


/* Own Variables */
private:
  static const uint16_t _id = 0; // unique bo scheme identifier

};



CLICK_ENDDECLS

#endif /* BO_OFF_HH */
