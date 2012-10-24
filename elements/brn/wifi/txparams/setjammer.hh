#ifndef CLICK_SETJAMMER_HH
#define CLICK_SETJAMMER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/string.hh>
#include <click/hashtable.hh>
#include <click/etheraddress.hh>
#include "elements/brn/brnelement.hh"

CLICK_DECLS

class SetJammer : public BRNElement {

public:
	#define RTS_CTS_STRATEGY_ALWAYS_OFF 0 //default
	#define RTS_CTS_STRATEGY_ALWAYS_ON 1
	#define RTS_CTS_STRATEGY_RANDOM 2


	SetJammer();
	~SetJammer();

	const char *class_name() const		{ return "SetJammer"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }
	

	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);

	Packet *simple_action(Packet *);

	void add_handlers();

  void set_cca(int cs_threshold, int rx_treshold, int cp_treshold);
  String read_cca();

  bool _jammer;
};

CLICK_ENDDECLS
#endif
