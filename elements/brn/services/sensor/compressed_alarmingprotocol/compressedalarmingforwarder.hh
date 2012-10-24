#ifndef COMPRESSEDALARMINGFORWARDERELEMENT_HH
#define COMPRESSEDALARMINGFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

#include "compressedalarmingstate.hh"
#include "compressedalarmingprotocol.hh"

CLICK_DECLS

class CompressedAlarmingForwarder : public BRNElement {

 public:

  //
  //methods
  //
  CompressedAlarmingForwarder();
  ~CompressedAlarmingForwarder();

  const char *class_name() const  { return "CompressedAlarmingForwarder"; }
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

  CompressedAlarmingState *_as;
  BRN2NodeIdentity *_nodeid;

  uint8_t alarm_id;

  uint8_t _ttl_limit;

  bool _rssi_delay;
};

CLICK_ENDDECLS
#endif
