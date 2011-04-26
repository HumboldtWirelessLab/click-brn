#ifndef BATMANORIGINATORFORWARDERELEMENT_HH
#define BATMANORIGINATORFORWARDERELEMENT_HH

#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include "batmanroutingtable.hh"

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"

CLICK_DECLS

#define QUEUE_DELAY 10

class BatmanOriginatorForwarder : public BRNElement {

 public:

  //
  //methods
  //
  BatmanOriginatorForwarder();
  ~BatmanOriginatorForwarder();

  const char *class_name() const  { return "BatmanOriginatorForwarder"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  void run_timer(Timer *timer);

  typedef Vector<Packet *> SendBuffer;

 private:
  //
  //member
  //
  BatmanRoutingTable *_brt;
  BRN2NodeIdentity *_nodeid;

  Timer _sendbuffer_timer;
  SendBuffer _packet_queue;

};

CLICK_ENDDECLS
#endif
