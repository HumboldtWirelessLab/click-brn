#ifndef ALARMINGRETRANSMITELEMENT_HH
#define ALARMINGRETRANSMITELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"

#include "alarmingstate.hh"

#define ALARMING_DEFAULT_RETRANMIT_TIME 100

CLICK_DECLS

class AlarmingRetransmit : public BRNElement {

 public:

  //
  //methods
  //
  AlarmingRetransmit();
  ~AlarmingRetransmit();

  const char *class_name() const  { return "AlarmingRetransmit"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void run_timer(Timer *t);

  void retransmit_active(bool active);
  void retransmit_alarm();

 private:
  //
  //member
  //

 BRN2NodeIdentity *_nodeid;

  Timer _timer;

  AlarmingState *_as;

  bool _alarm_active;

  int _retransmit_time;
};

CLICK_ENDDECLS
#endif
