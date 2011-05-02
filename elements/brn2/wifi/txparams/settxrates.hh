#ifndef CLICK_SETTXRATES_HH
#define CLICK_SETTXRATES_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

class SetTXRates : public Element { public:

  SetTXRates();
  ~SetTXRates();

  const char *class_name() const	{ return "SetTXRates"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();

  bool _debug;
 private:

   int _rate0,_rate1,_rate2,_rate3;
   int _tries0,_tries1,_tries2,_tries3;
   bool _mcs; //TODO: handle mcs for each rate separately

};

CLICK_ENDDECLS
#endif
