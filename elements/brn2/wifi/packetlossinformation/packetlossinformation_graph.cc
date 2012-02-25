/*
 * Packetlossinformation generate a Packet-Loss-Graph 
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "packetlossinformation_graph.hh"

CLICK_DECLS


PacketLossInformation_Graph::PacketLossInformation_Graph()
{

}
PacketLossInformation_Graph::~PacketLossInformation_Graph()
{

}
void PacketLossInformation_Graph::packetlossreason_set(String name,ATTRIBUTE feature, unsigned int value)
{
	PacketLossReason *ptr_reason = reason_get(name);
	if(NULL != ptr_reason) {
		if(feature == FRACTION) ptr_reason->setFraction(value);
		else if(feature == LABEL) ptr_reason->getLabel();
		else if(feature == ID) ptr_reason->setID(ptr_reason->id_get(value));

	}
}
void PacketLossInformation_Graph::packetlossreason_set(String name,ATTRIBUTE feature, void *ptr)
{
	PacketLossReason *ptr_reason = reason_get(name);
	if(NULL != ptr_reason) {
		if(feature == ADDSTATS) ptr_reason->setStatistics(ptr);
	}
}
int PacketLossInformation_Graph::overall_fract(PacketLossReason* ptr_node,int depth)
{
	return ptr_node->overall_fract(depth);
}


PacketLossReason* PacketLossInformation_Graph::reason_get(PacketLossReason::PossibilityE id)
{
	return	map_poss_packetlossreson.get(id);
}

PacketLossReason::PossibilityE PacketLossInformation_Graph::possibility_get(String name)
{
	if(name.compare("packet_loss") == 0){  return PacketLossReason::PACKET_LOSS;}
	else if(name.compare("interference") == 0){ return PacketLossReason::INTERFERENCE;}
	else if(name.compare("channel_fading") == 0){ return PacketLossReason::CHANNEL_FADING;}
	else if(name.compare("wifi") == 0){ return PacketLossReason::WIFI;}
	else if(name.compare("non_wifi") == 0){ return PacketLossReason::NON_WIFI;}
	else if(name.compare("weak_signal") == 0){ return PacketLossReason::WEAK_SIGNAL;}
	else if(name.compare("shadowing") == 0){ return PacketLossReason::SHADOWING;}
	else if(name.compare("multipath_fading") == 0){ return PacketLossReason::MULTIPATH_FADING;}
	else if(name.compare("co_channel") == 0){ return PacketLossReason::CO_CHANNEL;}
	else if(name.compare("adjacent_channel") == 0){ return PacketLossReason::ADJACENT_CHANNEL;}
	else if(name.compare("g_vs_b") == 0){return PacketLossReason::G_VS_B;}
	else if(name.compare("narrowband") == 0){return PacketLossReason::NARROWBAND;}
	else if(name.compare("broadband") == 0){return PacketLossReason::BROADBAND;}
	else if(name.compare("in_range") == 0){return PacketLossReason::IN_RANGE;}
	else if(name.compare("hidden_node") == 0){return PacketLossReason::HIDDEN_NODE;}
	else if(name.compare("narrowband_cooperative") == 0){return PacketLossReason::NARROWBAND_COOPERATIVE;}
	else if(name.compare("narrowband_non_cooperative") == 0){return PacketLossReason::NARROWBAND_NON_COOPERATIVE;}
	else if(name.compare("broadband_cooperative") == 0){return PacketLossReason::BROADBAND_COOPERATIVE;}
	else if(name.compare("broadband_non_cooperative") == 0){return PacketLossReason::BROADBAND_NON_COOPERATIVE;}
}



PacketLossReason* PacketLossInformation_Graph::reason_get(String name)
{
	PacketLossReason::PossibilityE id =  possibility_get(name);
	return	reason_get(id);
}

PacketLossReason* PacketLossInformation_Graph::root_node_get()
{ 
	return _root;
}


String PacketLossInformation_Graph::print() 
{
	StringAccum sa;
	for (HashTable<int,PacketLossReason*>::iterator it = map_poss_packetlossreson.begin(); it; ++it) {
		sa << "\t\t" << it.value()->print() << "\n";

	}
	return sa.take_string();
}


void PacketLossInformation_Graph::graph_build()  
{
	// init nodes
	PacketLossReason *ptr_PACKET_LOSS_ROOT_NODE = new PacketLossReason;
	PacketLossReason *ptr_INTERFERENCE = new PacketLossReason;	
	PacketLossReason *ptr_CHANNEL_FADING = new PacketLossReason;	
	PacketLossReason *ptr_WIFI = new PacketLossReason;	
	PacketLossReason *ptr_NON_WIFI = new PacketLossReason;	
	PacketLossReason *ptr_WEAK_SIGNAL = new PacketLossReason;	
	PacketLossReason *ptr_SHADOWING = new PacketLossReason;	
	PacketLossReason *ptr_MULTIPATH_FADING = new PacketLossReason;	
	PacketLossReason *ptr_CO_CHANNEL = new PacketLossReason;	
	PacketLossReason *ptr_ADJACENT_CHANNEL = new PacketLossReason;	
	PacketLossReason *ptr_G_VS_B = new PacketLossReason;	
	PacketLossReason *ptr_NARROWBAND = new PacketLossReason;	
	PacketLossReason *ptr_BROADBAND = new PacketLossReason;	
	PacketLossReason *ptr_IN_RANGE = new PacketLossReason;	
	PacketLossReason *ptr_HIDDEN_NODE = new PacketLossReason;	
	PacketLossReason *ptr_NARROWBAND_COOPERATIVE = new PacketLossReason;	
	PacketLossReason *ptr_NARROWBAND_NON_COOPERATIVE = new PacketLossReason;	
	PacketLossReason *ptr_BROADBAND_COOPERATIVE = new PacketLossReason;	
	PacketLossReason *ptr_BROADBAND_NON_COOPERATIVE = new PacketLossReason;	

	//init root-node (see header file)
	 _root = ptr_PACKET_LOSS_ROOT_NODE;

	ptr_PACKET_LOSS_ROOT_NODE->setID(PacketLossReason::PACKET_LOSS);
	ptr_PACKET_LOSS_ROOT_NODE->setParent(NULL);
	ptr_PACKET_LOSS_ROOT_NODE->setChild(0,ptr_INTERFERENCE);
	ptr_PACKET_LOSS_ROOT_NODE->setChild(1,ptr_CHANNEL_FADING);
	map_poss_packetlossreson.set(ptr_PACKET_LOSS_ROOT_NODE->getID(),ptr_PACKET_LOSS_ROOT_NODE);

	ptr_INTERFERENCE->setID(PacketLossReason::INTERFERENCE);
 	ptr_INTERFERENCE->setParent(ptr_PACKET_LOSS_ROOT_NODE);
 	ptr_INTERFERENCE->setChild(0,ptr_WIFI);
	ptr_INTERFERENCE->setChild(1,ptr_NON_WIFI);
	map_poss_packetlossreson.set(ptr_INTERFERENCE->getID(),ptr_INTERFERENCE);

	ptr_CHANNEL_FADING->setID(PacketLossReason::CHANNEL_FADING);
 	ptr_CHANNEL_FADING->setParent(ptr_PACKET_LOSS_ROOT_NODE);
	ptr_CHANNEL_FADING->setChild(0,ptr_WEAK_SIGNAL);
	ptr_CHANNEL_FADING->setChild(1,ptr_SHADOWING);
	ptr_CHANNEL_FADING->setChild(2,ptr_MULTIPATH_FADING);
	map_poss_packetlossreson.set(ptr_CHANNEL_FADING->getID(),ptr_CHANNEL_FADING);

	ptr_WIFI->setID(PacketLossReason::WIFI);
	ptr_WIFI->setParent(ptr_INTERFERENCE);
	ptr_WIFI->setChild(0,ptr_CO_CHANNEL);
	ptr_WIFI->setChild(1,ptr_ADJACENT_CHANNEL);
	ptr_WIFI->setChild(2,ptr_G_VS_B);
	map_poss_packetlossreson.set(ptr_WIFI->getID(),ptr_WIFI);

	ptr_NON_WIFI->setID(PacketLossReason::NON_WIFI);
 	ptr_NON_WIFI->setParent(ptr_INTERFERENCE);
	ptr_NON_WIFI->setChild(0,ptr_NARROWBAND);
	ptr_NON_WIFI->setChild(1,ptr_BROADBAND);
	map_poss_packetlossreson.set(ptr_NON_WIFI->getID(),ptr_NON_WIFI);

	ptr_WEAK_SIGNAL->setID(PacketLossReason::WEAK_SIGNAL);
 	ptr_WEAK_SIGNAL->setParent(ptr_CHANNEL_FADING);
	map_poss_packetlossreson.set(ptr_WEAK_SIGNAL->getID(),ptr_WEAK_SIGNAL);

	ptr_SHADOWING->setID(PacketLossReason::SHADOWING);
 	ptr_SHADOWING->setParent(ptr_CHANNEL_FADING);
	map_poss_packetlossreson.set(ptr_SHADOWING->getID(),ptr_SHADOWING);

	ptr_MULTIPATH_FADING->setID(PacketLossReason::MULTIPATH_FADING);
 	ptr_MULTIPATH_FADING->setParent(ptr_CHANNEL_FADING);
	map_poss_packetlossreson.set(ptr_MULTIPATH_FADING->getID(),ptr_MULTIPATH_FADING);

	ptr_CO_CHANNEL->setID(PacketLossReason::CO_CHANNEL);
 	ptr_CO_CHANNEL->setParent(ptr_WIFI);
	ptr_CO_CHANNEL->setChild(0,ptr_IN_RANGE);
	ptr_CO_CHANNEL->setChild(1,ptr_HIDDEN_NODE);
	map_poss_packetlossreson.set(ptr_CO_CHANNEL->getID(),ptr_CO_CHANNEL);

	ptr_ADJACENT_CHANNEL->setID(PacketLossReason::ADJACENT_CHANNEL);
 	ptr_ADJACENT_CHANNEL->setParent(ptr_WIFI);
	map_poss_packetlossreson.set(ptr_ADJACENT_CHANNEL->getID(),ptr_ADJACENT_CHANNEL);

	ptr_G_VS_B->setID(PacketLossReason::G_VS_B);
 	ptr_G_VS_B->setParent(ptr_WIFI);
	map_poss_packetlossreson.set(ptr_G_VS_B->getID(),ptr_G_VS_B);

	ptr_NARROWBAND->setID(PacketLossReason::NARROWBAND);
 	ptr_NARROWBAND->setParent(ptr_NON_WIFI);
	ptr_NARROWBAND->setChild(0,ptr_NARROWBAND_COOPERATIVE);
	ptr_NARROWBAND->setChild(1,ptr_NARROWBAND_NON_COOPERATIVE);
	map_poss_packetlossreson.set(ptr_NARROWBAND->getID(),ptr_NARROWBAND);

	ptr_BROADBAND->setID(PacketLossReason::BROADBAND);
 	ptr_BROADBAND->setParent(ptr_NON_WIFI);
	ptr_BROADBAND->setChild(0,ptr_BROADBAND_COOPERATIVE);
	ptr_BROADBAND->setChild(1,ptr_BROADBAND_NON_COOPERATIVE);
	map_poss_packetlossreson.set(ptr_BROADBAND->getID(),ptr_BROADBAND);

	ptr_IN_RANGE->setID(PacketLossReason::IN_RANGE);
 	ptr_IN_RANGE->setParent(ptr_CO_CHANNEL);
	map_poss_packetlossreson.set(ptr_IN_RANGE->getID(),ptr_IN_RANGE);

	ptr_HIDDEN_NODE->setID(PacketLossReason::HIDDEN_NODE);
 	ptr_HIDDEN_NODE->setParent(ptr_CO_CHANNEL);
	map_poss_packetlossreson.set(ptr_HIDDEN_NODE->getID(),ptr_HIDDEN_NODE);

	ptr_NARROWBAND_COOPERATIVE->setID(PacketLossReason::NARROWBAND_COOPERATIVE);
 	ptr_NARROWBAND_COOPERATIVE->setParent(ptr_NARROWBAND);
	map_poss_packetlossreson.set(ptr_NARROWBAND_COOPERATIVE->getID(),ptr_NARROWBAND_COOPERATIVE);

	ptr_NARROWBAND_NON_COOPERATIVE->setID(PacketLossReason::NARROWBAND_NON_COOPERATIVE);
 	ptr_NARROWBAND_NON_COOPERATIVE->setParent(ptr_NARROWBAND);
	map_poss_packetlossreson.set(ptr_NARROWBAND_NON_COOPERATIVE->getID(),ptr_NARROWBAND_NON_COOPERATIVE);

	ptr_BROADBAND_COOPERATIVE->setID(PacketLossReason::BROADBAND_COOPERATIVE);
 	ptr_BROADBAND_COOPERATIVE->setParent(ptr_BROADBAND);
	map_poss_packetlossreson.set(ptr_BROADBAND_COOPERATIVE->getID(),ptr_BROADBAND_COOPERATIVE);

	ptr_BROADBAND_NON_COOPERATIVE->setID(PacketLossReason::BROADBAND_NON_COOPERATIVE);
 	ptr_BROADBAND_NON_COOPERATIVE->setParent(ptr_BROADBAND);
	map_poss_packetlossreson.set(ptr_BROADBAND_NON_COOPERATIVE->getID(),ptr_BROADBAND_NON_COOPERATIVE);
}
CLICK_ENDDECLS
ELEMENT_REQUIRES(PacketLossReason)
ELEMENT_PROVIDES(PacketLossInformation_Graph)
