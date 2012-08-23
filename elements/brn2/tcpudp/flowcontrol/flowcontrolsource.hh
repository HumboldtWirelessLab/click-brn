#ifndef FLOWCONTROLSOURCEELEMENT_HH
#define FLOWCONTROLSOURCEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class FlowControlSource : public BRNElement
{

 public:
  //
  //methods
  //
  FlowControlSource();
  ~FlowControlSource();

  const char *class_name() const  { return "FlowControlSource"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "2/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
   
};

CLICK_ENDDECLS
#endif
