#ifndef CLICK_BRN2_SETRTSCTS_HH
#define CLICK_BRN2_SETRTSCTS_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include "elements/brn2/brnelement.hh"
#include <click/string.hh>

CLICK_DECLS

class Brn2_SetRTSCTS : public BRNElement { 

public:

	Brn2_SetRTSCTS();
	~Brn2_SetRTSCTS();

	const char *class_name() const		{ return "Brn2_SetRTSCTS"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }
	
//	int configure(Vector<String> &conf, ErrorHandler *errh);

	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);

	Packet *simple_action(Packet *);
	void add_handlers();
	/* true = on, else off*/
	void rts_cts_decision();
	bool is_number_in_random_range(unsigned int value); //range [0,100]

	bool rts_get();
	void rts_set(bool value);


	//void dest_test(Packet *p);
	Packet * dest_test(Packet *p);


private:	
	bool _rts;
	unsigned _mode;
        EtherAddress _bssid;
	class WirelessInfo *_winfo;

};

CLICK_ENDDECLS
#endif
