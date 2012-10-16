#ifndef COMPRESSEDALARMINGRETRANSMITELEMENT_HH
#define COMPRESSEDALARMINGRETRANSMITELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

#include "compressedalarmingstate.hh"

#define ALARMING_DEFAULT_RETRANMIT_TIME 100

CLICK_DECLS

class CompressedAlarmingRetransmit : public BRNElement {

 public:

  //
  //methods
  //
  CompressedAlarmingRetransmit();
  ~CompressedAlarmingRetransmit();

  const char *class_name() const  { return "CompressedAlarmingRetransmit"; }
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

  CompressedAlarmingState *_as;

  bool _alarm_active;

  int _retransmit_time;
};

CLICK_ENDDECLS
#endif
