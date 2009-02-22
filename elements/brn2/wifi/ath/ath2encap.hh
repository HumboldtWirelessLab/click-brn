#ifndef CLICK_ATH2ENCAP_HH
#define CLICK_ATH2ENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include "ath2_desc.h"

CLICK_DECLS

class Ath2Encap : public Element { public:

  Ath2Encap();
  ~Ath2Encap();

  const char *class_name() const  { return "Ath2Encap"; }
  const char *port_count() const  { return PORTS_1_1; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();

  bool _debug;

 private:

  bool _athencap;

};

CLICK_ENDDECLS
#endif
