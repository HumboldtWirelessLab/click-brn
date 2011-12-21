#ifndef CLICK_PacketLossInformation_HH
#define CLICK_PacketLossInformation_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include "elements/brn2/brnelement.hh"
#include "packetlossreason.hh"

CLICK_DECLS

class PacketLossInformation : public BRNElement { public:

	PacketLossInformation();
	~PacketLossInformation();

	const char *class_name() const		{ return "PacketLossInformation"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);
	void test();
	void write_test_id(PacketLossReason::PossibilityE id);
	void write_test_obj(PacketLossReason* ptr_obj);

  PacketLossReason *_root;
  PacketLossReason *get_reason(PacketLossReason::PossibilityE id);

};

CLICK_ENDDECLS
#endif
