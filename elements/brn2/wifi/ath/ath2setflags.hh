#ifndef CLICK_ATH2SETFLAGS_HH
#define CLICK_ATH2SETFLAGS_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

class Ath2SetFlags : public Element { public:

  Ath2SetFlags();
  ~Ath2SetFlags();

  const char *class_name() const	{ return "Ath2SetFlags"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();

  bool _debug;

 private:

  int32_t _queue;
};

CLICK_ENDDECLS
#endif
