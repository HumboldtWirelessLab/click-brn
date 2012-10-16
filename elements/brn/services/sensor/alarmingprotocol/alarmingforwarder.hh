#ifndef ALARMINGFORWARDERELEMENT_HH
#define ALARMINGFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"

#include "alarmingstate.hh"
#include "alarmingprotocol.hh"

CLICK_DECLS

class AlarmingForwarder : public BRNElement {

 public:

  //
  //methods
  //
  AlarmingForwarder();
  ~AlarmingForwarder();

  const char *class_name() const  { return "AlarmingForwarder"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //

  AlarmingState *_as;
  BRN2NodeIdentity *_nodeid;

  uint8_t alarm_id;

  uint8_t _ttl_limit;

  bool _rssi_delay;
};

CLICK_ENDDECLS
#endif
