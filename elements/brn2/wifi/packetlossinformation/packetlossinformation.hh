#ifndef CLICK_PacketLossInformation_HH
#define CLICK_PacketLossInformation_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include "elements/brn2/brnelement.hh"
#include "packetlossreason.hh"
#include <click/hashtable.hh>
#include <click/string.hh>

CLICK_DECLS

class PacketLossInformation : public BRNElement { 

public:

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
	PacketLossReason* get_root_node();
	PacketLossReason* get_reason_by_id(PacketLossReason::PossibilityE id);
	/*
	"packet_loss"
	"interference"
	"channel_fading"
	"wifi"
	"non_wifi"
	"weak_signal"
	"shadowing"
	"multipath_fading"
	"co_channel"
	"adjacent_channel"
	"g_vs_b"i
	"narrowband"
	"broadband"
	"in_range"
	"hidden_node"
	"narrowband_cooperative"
	"narrowband_non_cooperative"
	"broadband_cooperative"
	"broadband_non_cooperative"
	*/
	PacketLossReason* get_reason_by_name(String name);
	PacketLossReason::PossibilityE get_possibility_by_name(String name);
	int overall_fract(PacketLossReason* ptr_node,int depth);

private:
	PacketLossReason *_root;
	HashTable<int,PacketLossReason*> map_poss_packetlossreson;


};

CLICK_ENDDECLS
#endif
