#ifndef CLICK_PACKETLOSSINFORMATION_GRAPH_HH
#define CLICK_PACKETLOSSINFORMATION_GRAPH_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include "elements/brn2/brnelement.hh"
#include "packetlossreason.hh"
#include <click/hashtable.hh>
#include <click/string.hh>

CLICK_DECLS

class PacketLossInformation_Graph{ 

public:

	PacketLossInformation_Graph();
	~PacketLossInformation_Graph();
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
	void graph_build();
//	void graph_print(PacketLossReason *ptr_PACKET_LOSS_ROOT_NODE);


private:
	PacketLossReason *_root;
	HashTable<int,PacketLossReason*> map_poss_packetlossreson;


};

CLICK_ENDDECLS
#endif
