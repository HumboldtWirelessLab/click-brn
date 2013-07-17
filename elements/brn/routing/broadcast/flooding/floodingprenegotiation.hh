#ifndef FLOODINGPRENEGOTIATIONELEMENT_HH
#define FLOODINGPRENEGOTIATIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"

CLICK_DECLS

#define FLOODING_PRENEGOTIATION_STARTTIME 100000 /*ms (100s)*/
class FloodingPrenegotiation : public BRNElement {

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
  
  uint32_t _start_time;
  Timestamp _start_ts;
  bool _active;
};

CLICK_ENDDECLS
#endif
