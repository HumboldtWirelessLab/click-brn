/*
 * Packetlossinformation generate a Packet-Loss-Graph 
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>


#include "packetlossinformation.hh"

CLICK_DECLS

PacketLossInformation::PacketLossInformation()
{
_debug = 4;
}

PacketLossInformation::~PacketLossInformation()
{
}


int PacketLossInformation::initialize(ErrorHandler *)
{
	return 0;

}

int PacketLossInformation::configure(Vector<String> &conf, ErrorHandler* errh)
{
	if (cp_va_kparse(conf, this, errh,
		"DEBUG", cpkP, cpInteger, &_debug,
	      cpEnd) < 0) return -1;

	return 0;
}

unsigned int PacketLossInformation::neighbours_number_get()
{
	return node_neighbours.size()-1; //without Broadcast-Address
}

void PacketLossInformation::graph_insert(EtherAddress pkt_address)
{
	if (NULL != graph_get(pkt_address)) {
		PacketLossInformation_Graph *ptr_graph = new PacketLossInformation_Graph();
		ptr_graph->graph_build();
		graph_set(pkt_address, ptr_graph);
		EtherAddress broadcast_address = EtherAddress();
		broadcast_address = broadcast_address.make_broadcast();
		BRN_DEBUG("Ist Pointer Address? %d",broadcast_address.is_broadcast());

		PacketLossInformation_Graph *ptr_graph_broadcast = new PacketLossInformation_Graph();
		graph_set(broadcast_address,ptr_graph_broadcast);
	}
}

void PacketLossInformation::graph_delete(EtherAddress pkt_address)
{
	PacketLossInformation_Graph *ptr_graph = graph_get(pkt_address);
	if (NULL != ptr_graph) {
		node_neighbours.erase(pkt_address);
		delete ptr_graph;	
	}
}


void PacketLossInformation::graph_set(EtherAddress pkt_address, PacketLossInformation_Graph* pli_graph)
{
	node_neighbours[pkt_address] = pli_graph;
}



PacketLossInformation_Graph* PacketLossInformation::graph_get(EtherAddress pkt_address)
{
	BRN_DEBUG("I am in PacketLossInformation::graph_get(EtherAddress pkt_address)");
	return	node_neighbours.get(pkt_address);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(PacketLossInformation_Graph)
EXPORT_ELEMENT(PacketLossInformation)
