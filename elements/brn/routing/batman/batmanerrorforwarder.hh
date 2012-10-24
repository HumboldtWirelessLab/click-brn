#ifndef BATMANERRORFORWARDERELEMENT_HH
#define BATMANERRORFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn/brnelement.hh"
#include "batmanroutingtable.hh"
#include "batmanprotocol.hh"


CLICK_DECLS

class BatmanErrorForwarder : public BRNElement {

 public:

  //
  //methods
  //
  BatmanErrorForwarder();
  ~BatmanErrorForwarder();

  const char *class_name() const  { return "BatmanErrorForwarder"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //

  BatmanRoutingTable *_brt;
  BRN2NodeIdentity *_nodeid;

};

CLICK_ENDDECLS
#endif
