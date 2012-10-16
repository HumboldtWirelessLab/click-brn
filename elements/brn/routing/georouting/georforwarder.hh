#ifndef GEORFORWARDERELEMENT_HH
#define GEORFORWARDERELEMENT_HH

#include "elements/brn2/brnelement.hh"
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "geortable.hh"
#include "georprotocol.hh"

CLICK_DECLS

class GeorForwarder : public BRNElement {

 public:

  //
  //methods
  //
  GeorForwarder();
  ~GeorForwarder();

  const char *class_name() const  { return "GeorForwarder"; }
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

  GeorTable *_rt;
  BRN2NodeIdentity *_nodeid;

};

CLICK_ENDDECLS
#endif
