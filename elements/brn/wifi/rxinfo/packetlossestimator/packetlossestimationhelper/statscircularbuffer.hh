/* 
 * File:   statscircularbuffer.hh
 * Author: kuehn@informatik.hu-berlin.de
 *
 * Created on 20. Juli 2012, 14:01
 */

#ifndef STATSCIRCULARBUFFER_HH
#define	STATSCIRCULARBUFFER_HH
#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>
#if CLICK_NS
	#include <click/router.hh>
#endif
#include <click/hashmap.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include "elements/brn/brnelement.hh"

#include "elements/brn/wifi/rxinfo/packetlossestimator/packetlossinformation/packetlossinformation.hh"
#include "packetparameter.hh"
#include "packetlossstatistics.hh"

CLICK_DECLS

class StatsCircularBuffer {

public:
	StatsCircularBuffer(const uint16_t);
	virtual ~StatsCircularBuffer();
	void insert_values(PacketParameter &, PacketLossInformation &);
	//< Insert Value without setting a timestamp
	void insert_values_wo_time(PacketParameter &, PacketLossInformation &);
	Vector<PacketLossStatistics> get_values(EtherAddress &, uint16_t);
	void set_buffer_size(uint16_t size);
	uint16_t get_buffer_size();
	Vector<PacketLossStatistics> get_all_values(EtherAddress &);
	Vector<EtherAddress> get_stored_addresses();
	void remove_stored_address(EtherAddress &);
	bool exists_address(EtherAddress &);

private:
	StatsCircularBuffer();
	void insert_values(PacketParameter &, PacketLossInformation &, bool);

	uint16_t buffer_size;
	HashMap<EtherAddress, Vector<PacketLossStatistics> > ether_address_time_map;
};

CLICK_ENDDECLS
#endif	/* STATSCIRCULARBUFFER_HH */
