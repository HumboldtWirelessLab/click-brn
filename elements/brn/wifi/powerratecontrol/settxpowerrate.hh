#ifndef CLICK_SETTXPOWERRATE_HH
#define CLICK_SETTXPOWERRATE_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <elements/wifi/availablerates.hh>

#include "powerratestats.hh"

CLICK_DECLS

class SetTXPowerRate : public Element { public:

  SetTXPowerRate();
  ~SetTXPowerRate();

  const char *class_name() const  { return "SetTXPowerRate"; }
  const char *port_count() const  { return "3/3"; }

  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  void push( int port, Packet *packet );

  void add_handlers();

  bool _debug;

 private:

  AvailableRates *_rtable;
  int _maxpower;
  PowerRateStats _stats;
};

CLICK_ENDDECLS
#endif