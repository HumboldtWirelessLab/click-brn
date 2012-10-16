#ifndef EEWSSOURCEELEMENT_HH
#define EEWSSOURCEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn/brnelement.hh"
#include "eewsstate.hh"

CLICK_DECLS

class EewsSource : public BRNElement {

 public:

  //
  //methods
  //
  EewsSource();
  ~EewsSource();

  const char *class_name() const  { return "EewsSource"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void send_alarm(uint8_t alarm_type);

 private:
  //
  //member
  //

  EewsState *_as;
};

CLICK_ENDDECLS
#endif
