#ifndef BATMANFORWARDERELEMENT_HH
#define BATMANFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include "batmanroutingtable.hh"
#include "batmanprotocol.hh"

/** TODO: reactivate BRNLogger
*/

CLICK_DECLS

class BatmanForwarder : public Element {

 public:

  //
  //methods
  //
  BatmanForwarder();
  ~BatmanForwarder();

  const char *class_name() const  { return "BatmanForwarder"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/3"; }

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

 public:
  int _debug;

};

CLICK_ENDDECLS
#endif
