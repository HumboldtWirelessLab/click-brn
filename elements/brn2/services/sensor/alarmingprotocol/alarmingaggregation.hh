#ifndef ALARMINGAGGREGATIONELEMENT_HH
#define ALARMINGAGGREGATIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/standard/randomdelayqueue.hh"

#include "alarmingstate.hh"
#include "alarmingprotocol.hh"
#include "alarmingretransmit.hh"

CLICK_DECLS

class AlarmingAggregation : public BRNElement {

 public:

  //
  //methods
  //
  AlarmingAggregation();
  ~AlarmingAggregation();

  const char *class_name() const  { return "AlarmingAggregation"; }

  const char *port_count() const              { return PORTS_1_1X2; }
  const char *processing() const              { return "l/lh"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  Packet* pull(int port);

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //

  AlarmingState *_as;

  RandomDelayQueue *_delay_queue;
  AlarmingRetransmit *_alarm_ret;

  int _no_agg;
  int _no_pkts;
};

CLICK_ENDDECLS
#endif
