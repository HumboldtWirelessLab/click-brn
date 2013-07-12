#ifndef FLOODINGPRENEGOTIATIONELEMENT_HH
#define FLOODINGPRENEGOTIATIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/routing/linkstat/metric/brn2_brnetxmetric.hh"

CLICK_DECLS

class FloodingPrenegotiation : public Element {

 public:
  //
  //methods
  //
  FloodingPrenegotiation();
  ~FloodingPrenegotiation();

  const char *class_name() const  { return "FloodingPrenegotiation"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  int lpSendHandler(char *buffer, int size);
  int lpReceiveHandler(char *buffer, int size);

  bool _debug;
};

CLICK_ENDDECLS
#endif
