//#ifndef CLICK_PacketLossInformation_HH
//#define CLICK_PacketLossInformation_HH
#ifndef CLICK_PACKETLOSSINFORMATION_HH
#define CLICK_PACKETLOSSINFORMATION_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include "elements/brn2/brnelement.hh"
#include "packetlossinformation_graph.hh"
#include <click/hashtable.hh>
#include <click/string.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>

CLICK_DECLS

class PacketLossInformation : public BRNElement{ 

public:

	PacketLossInformation();
	~PacketLossInformation();

	const char *class_name() const		{ return "PacketLossInformation"; }
	const char *port_count() const		{ return "0/0"; }
	const char *processing() const		{ return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);

	PacketLossInformation_Graph* graph_insert(EtherAddress pkt_address);
	void graph_delete(EtherAddress pkt_address);

	PacketLossInformation_Graph* graph_get(EtherAddress pkt_address);
	void graph_set(EtherAddress pkt_address,PacketLossInformation_Graph* pli_graph);

	typedef Vector<EtherAddress> NeighboursAddressesVector;
	NeighboursAddressesVector node_neighbours_addresses_get();
	typedef NeighboursAddressesVector::const_iterator NeighboursAddressesIterator;
	void print();
	unsigned int neighbours_number_get();


private:
	HashTable<EtherAddress,PacketLossInformation_Graph*> node_neighbours;
	NeighboursAddressesVector node_neighbours_vector;

};

CLICK_ENDDECLS
#endif
