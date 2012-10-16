#ifndef COMPRESSEDALARMINGSOURCEELEMENT_HH
#define COMPRESSEDALARMINGSOURCEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "compressedalarmingstate.hh"

CLICK_DECLS

class CompressedAlarmingSource : public BRNElement {

 public:

  //
  //methods
  //
  CompressedAlarmingSource();
  ~CompressedAlarmingSource();

  const char *class_name() const  { return "CompressedAlarmingSource"; }
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

  CompressedAlarmingState *_as;
};

CLICK_ENDDECLS
#endif
