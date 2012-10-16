#ifndef EEWSAGGREGATIONELEMENT_HH
#define EEWSAGGREGATIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/standard/randomdelayqueue.hh"

#include "eewsstate.hh"
#include "eewsprotocol.hh"
#include "eewsretransmit.hh"

CLICK_DECLS

class EewsAggregation : public BRNElement {

 public:

  //
  //methods
  //
  EewsAggregation();
  ~EewsAggregation();

  const char *class_name() const  { return "EewsAggregation"; }

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

  EewsState *_as;

  RandomDelayQueue *_delay_queue;
  EewsRetransmit *_alarm_ret;

  int _no_agg;
  int _no_pkts;
};

CLICK_ENDDECLS
#endif
