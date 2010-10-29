#ifndef ALARMINGSOURCEELEMENT_HH
#define ALARMINGSOURCEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "alarmingstate.hh"

CLICK_DECLS

class AlarmingSource : public BRNElement {

 public:

  //
  //methods
  //
  AlarmingSource();
  ~AlarmingSource();

  const char *class_name() const  { return "AlarmingSource"; }
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

  AlarmingState *_as;
};

CLICK_ENDDECLS
#endif
