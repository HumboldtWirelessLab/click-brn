#ifndef CLICK_BRN2_SETRTSCTS_HH
#define CLICK_BRN2_SETRTSCTS_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include "elements/brn2/brnelement.hh"
#include <click/string.hh>
#include <click/hashtable.hh>
#include <click/etheraddress.hh>
#include <elements/brn2/wifi/packetlossinformation/packetlossinformation.hh>

CLICK_DECLS

class Brn2_SetRTSCTS : public BRNElement { 

public:
	#define RTS_CTS_STRATEGY_ALWAYS_OFF 0 //default
	#define RTS_CTS_STRATEGY_ALWAYS_ON 1
	#define RTS_CTS_STRATEGY_RANDOM 2


	Brn2_SetRTSCTS();
	~Brn2_SetRTSCTS();

	const char *class_name() const		{ return "Brn2_SetRTSCTS"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }
	

	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);

	Packet *simple_action(Packet *);

	void add_handlers();
	String  print();
	void reset();
	uint16_t strategy_get();
	void strategy_set(uint16_t value);

private:	
	bool _rts;//true:= rts on; false:=rts_off
	uint16_t _rts_cts_strategy;//RTS-CTS Strategy
	//total number of packets who got through this element
	unsigned int pkt_total;	
	struct _neighbour_statistics {
        	 uint32_t pkt_total;
		 uint32_t rts_on;
	};

	typedef struct _neighbour_statistics neighbour_statistics,*PNEIGHBOUR_STATISTICS;
	HashTable<EtherAddress,PNEIGHBOUR_STATISTICS> node_neighbours;
	PacketLossInformation *pli; //PacketLossInformation-Element (see:../packetlossinformation/packetlossinformation.hh)
	PNEIGHBOUR_STATISTICS neighbours_statistic_get(EtherAddress dst_address);	
	void neighbours_statistic_insert(EtherAddress dst_address);
	uint32_t neighbour_statistic_pkt_total_get(EtherAddress dst_address);
	uint32_t neighbour_statistic_rts_on_get(EtherAddress dst_address);
	uint32_t neighbour_statistc_rts_off_get(EtherAddress dst_address);
	void neighbours_statistic_set(EtherAddress dst_address,PNEIGHBOUR_STATISTICS ptr_neighbour_stats);
	Packet* dest_test(Packet *p);
	void address_broadcast_insert();
	bool rts_get();
	/* true = on, else off*/
	void rts_set(bool value);
	void rts_cts_decision(unsigned int value);
	bool is_number_in_random_range(unsigned int value); //range [0,100]
};

CLICK_ENDDECLS
#endif
