#ifndef CLICK_SETTXTRIES_HH
#define CLICK_SETTXTRIES_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

class SetTXTries : public Element { public:
  
  SetTXTries();
  ~SetTXTries();
  
  const char *class_name() const		{ return "SetTXTries"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();
  static String read_handler(Element *e, void *);  
  static int write_handler(const String &arg, Element *e,
			   void *, ErrorHandler *errh);
private:
  
  int _tries;
  unsigned _offset;
};

CLICK_ENDDECLS
#endif
