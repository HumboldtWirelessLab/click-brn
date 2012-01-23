/*
 * Packetlossinformation generate a Packet-Loss-Graph 
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
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
	PacketLossInformation_Graph *ptr_graph = new PacketLossInformation_Graph();
	ptr_graph->graph_build();
	return 0;
}


int PacketLossInformation::configure(Vector<String> &, ErrorHandler *)
{

	return 0;
}

void PacketLossInformation::graph_insert(EtherAddress pkt_address)
{
	PacketLossInformation_Graph *ptr_graph = new PacketLossInformation_Graph();
	ptr_graph->graph_build();
	graph_set(pkt_address, ptr_graph);
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
