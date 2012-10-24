#ifndef GEORQUERIERELEMENT_HH
#define GEORQUERIERELEMENT_HH

#include "elements/brn/brnelement.hh"

#include <click/etheraddress.hh>
#include <click/vector.hh>
#include "geortable.hh"
#include "georprotocol.hh"

CLICK_DECLS

class GeorQuerier : public BRNElement {

 public:

  //
  //methods
  //
  GeorQuerier();
  ~GeorQuerier();

  const char *class_name() const  { return "GeorQuerier"; }
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

  GeorTable *_rt;
  BRN2NodeIdentity *_nodeid;

};

CLICK_ENDDECLS
#endif
