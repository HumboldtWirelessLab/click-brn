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
	EtherAddress broadcast_address = EtherAddress();
	broadcast_address = broadcast_address.make_broadcast();
	BRN_DEBUG("Ist Pointer Address? %d",broadcast_address.is_broadcast());
	graph_insert(broadcast_address);
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

PacketLossInformation_Graph* PacketLossInformation::graph_insert(EtherAddress pkt_address)
{
	PacketLossInformation_Graph *ptr_graph = graph_get(pkt_address);
	if (NULL == ptr_graph) {
	    	BRN_DEBUG("PLI:Graph_INSERT = %s",pkt_address.unparse().c_str());
		ptr_graph = new PacketLossInformation_Graph();
		ptr_graph->graph_build();
		graph_set(pkt_address, ptr_graph);
		node_neighbours_vector.push_back(pkt_address);	
	}
	return ptr_graph;	
}

PacketLossInformation::NeighboursAddressesVector PacketLossInformation::node_neighbours_addresses_get()
{
	return node_neighbours_vector;
}

void PacketLossInformation::graph_delete(EtherAddress pkt_address)
{
	PacketLossInformation_Graph *ptr_graph = graph_get(pkt_address);
	if (NULL != ptr_graph) {
		delete ptr_graph;
		int pos = -1;
		bool try_delete = false;
		for (NeighboursAddressesIterator it =  node_neighbours_vector.begin();it!=node_neighbours_vector.end();++it) {
			++pos;
			if(pkt_address == *it)  {
				BRN_DEBUG("Found Address %s at Position = %d",(*it).unparse().c_str(), pos);
				try_delete = true;
				break;
			}
		}
		BRN_DEBUG("Position = %d",pos);
		if (try_delete) {
			node_neighbours_vector.erase(node_neighbours_vector.begin()+pos);
			BRN_DEBUG("Deleted Broadcast-Adress");
		}
	}
}

void PacketLossInformation::print()
{
	EtherAddress pkt_address = EtherAddress();
	pkt_address = pkt_address.make_broadcast();
	BRN_DEBUG("PLI->print: before Iteration Size: %d",node_neighbours_vector.size());
	for (NeighboursAddressesIterator it =  node_neighbours_vector.begin();it!=node_neighbours_vector.end(); ++it) {
		//BRN_DEBUG("I have found an etheradress: %s\n\n",(*it).unparse().c_str());
		BRN_DEBUG("I have found an etheradress: %s\n\n",(*it).unparse().c_str());

	}
	BRN_DEBUG("PLI->print: after Iteration");
	int pos = -1;
	bool try_delete = false;
	for (NeighboursAddressesIterator it =  node_neighbours_vector.begin();it!=node_neighbours_vector.end();++it) {
		++pos;
		if(pkt_address == *it)  {
			BRN_DEBUG("Found Address %s at Position = %d",(*it).unparse().c_str(), pos);
			try_delete = true;
			break;
		}
	}
	BRN_DEBUG("Position = %d",pos);
	if (try_delete) {
		node_neighbours_vector.erase(node_neighbours_vector.begin()+pos);
		BRN_DEBUG("Deleted Broadcast-Adress");
	}
}

bool PacketLossInformation::ether_address_exists(EtherAddress pkt_address)
{
	for (NeighboursAddressesIterator it =  node_neighbours_vector.begin();it!=node_neighbours_vector.end();++it){
		if(pkt_address == *it) return true; 
	}
	return false;

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
