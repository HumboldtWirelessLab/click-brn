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
	BRNElement::init();
}

PacketLossInformation::~PacketLossInformation()
{
}

void PacketLossInformation::address_broadcast_insert()
{
	EtherAddress broadcast_address = EtherAddress();
	broadcast_address = broadcast_address.make_broadcast();
	BRN_DEBUG("Ist Pointer Address? %d",broadcast_address.is_broadcast());
	graph_insert(broadcast_address);
}

int PacketLossInformation::initialize(ErrorHandler *)
{
	address_broadcast_insert();
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
		unsigned int pos = 0;
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

bool PacketLossInformation::mac_address_exists(EtherAddress pkt_address)
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


String  PacketLossInformation::print()
{

	StringAccum sa;
	sa << "<brnelement name=\"PacketLossInformation\" myaddress=\""<< BRN_NODE_NAME << "\">\n";
	for (NeighboursAddressesIterator it =  node_neighbours_vector.begin();it!=node_neighbours_vector.end(); ++it) {
		sa << "\t<neighbour address=\"" << (*it).unparse().c_str() <<"\">\n";
		PacketLossInformation_Graph* ptr_graph = graph_get(*it);
		sa << ptr_graph->print();
		sa.append("\t</neighbour>\n");
	}
	sa.append("</brnelement>\n");
	return sa.take_string();
}

void PacketLossInformation::reset()
{
	for (NeighboursAddressesIterator it =  node_neighbours_vector.begin();it!=node_neighbours_vector.end(); ++it) {

		graph_delete(*it);
	}
	address_broadcast_insert();
}

void PacketLossInformation::pli_global_set(String reason, PacketLossInformation_Graph::ATTRIBUTE attrib, unsigned int value)
{
	EtherAddress broadcast_address = EtherAddress();
	broadcast_address = broadcast_address.make_broadcast();
	PacketLossInformation_Graph *ptr_graph = graph_get(broadcast_address);
	if (NULL != ptr_graph) {
			ptr_graph->packetlossreason_set(reason,attrib,value); 
	}
}

void PacketLossInformation::pli_global_set(String reason, PacketLossInformation_Graph::ATTRIBUTE attrib, void *ptr_value)
{
	EtherAddress broadcast_address = EtherAddress();
	broadcast_address = broadcast_address.make_broadcast();
	PacketLossInformation_Graph *ptr_graph = graph_get(broadcast_address);
	if (NULL != ptr_graph) {
			ptr_graph->packetlossreason_set(reason,attrib,ptr_value); 
	}
}


enum {H_PLI_PRINT,H_PLI_RESET,H_PLI_HIDDEN_NODE,H_PLI_INRANGE};

static String PacketLossInformation_read_param(Element *e, void *thunk)
{
	PacketLossInformation *f = (PacketLossInformation *)e;
	switch ((uintptr_t) thunk) {
		case H_PLI_PRINT:
			return f->print();
  			default:
				return String();
  	}
}

static int PacketLossInformation_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
	PacketLossInformation *f = (PacketLossInformation *)e;
	String s = cp_uncomment(in_s);
	unsigned int value = 0;
	switch((intptr_t)vparam) {
	  	case H_PLI_RESET: 
			f->reset();
		break;		
    		case H_PLI_HIDDEN_NODE: 
			if (!IntArg().parse(s, value)) return errh->error("stepup parameter must be unsigned");
			f->pli_global_set("hidden_node",PacketLossInformation_Graph::FRACTION,value);
		break;		
	  	case H_PLI_INRANGE: 
			if (!IntArg().parse(s, value)) return errh->error("stepup parameter must be unsigned");
			f->pli_global_set("in_range", PacketLossInformation_Graph::FRACTION,value);
		break;		
	  }
	  return 0;
}

void PacketLossInformation::add_handlers()
{
	BRNElement::add_handlers();
	add_read_handler("print", PacketLossInformation_read_param, H_PLI_PRINT);
	add_write_handler("reset",PacketLossInformation_write_param,H_PLI_RESET, Handler::h_button);
	add_write_handler("hidden_node",PacketLossInformation_write_param,H_PLI_HIDDEN_NODE);
	add_write_handler("inrange",PacketLossInformation_write_param,H_PLI_INRANGE);
}
CLICK_ENDDECLS
ELEMENT_REQUIRES(PacketLossInformation_Graph)
EXPORT_ELEMENT(PacketLossInformation)
