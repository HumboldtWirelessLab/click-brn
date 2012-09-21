#ifndef FLOWCONTROLSINKELEMENT_HH
#define FLOWCONTROLSINKELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn2/brnelement.hh"

#include "flowcontrolprotocol.hh"
#include "flowcontrolinfo.hh"

CLICK_DECLS


class FlowControlSink : public BRNElement
{
  
 public:
  //
  //methods
  //
  FlowControlSink();
  ~FlowControlSink();

  const char *class_name() const  { return "FlowControlSink"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  void clear_flowtab();
 
 private:
  FlowTable _flowtab;
};

CLICK_ENDDECLS
#endif
