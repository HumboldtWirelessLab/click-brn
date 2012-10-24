#ifndef COMPRESSEDALARMINGAGGREGATIONELEMENT_HH
#define COMPRESSEDALARMINGAGGREGATIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/standard/randomdelayqueue.hh"

#include "compressedalarmingstate.hh"
#include "compressedalarmingprotocol.hh"
#include "compressedalarmingretransmit.hh"

CLICK_DECLS

class CompressedAlarmingAggregation : public BRNElement {

 public:

  //
  //methods
  //
  CompressedAlarmingAggregation();
  ~CompressedAlarmingAggregation();

  const char *class_name() const  { return "CompressedAlarmingAggregation"; }

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

  CompressedAlarmingState *_as;

  RandomDelayQueue *_delay_queue;
  CompressedAlarmingRetransmit *_alarm_ret;

  int _no_agg;
  int _no_pkts;
};

CLICK_ENDDECLS
#endif
