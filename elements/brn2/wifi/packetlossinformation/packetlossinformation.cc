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
	PacketLossInformation::test();
	return 0;
}


int PacketLossInformation::configure(Vector<String> &, ErrorHandler *)
{

	return 0;
}


void PacketLossInformation::write_test_id(PacketLossReason::PossibilityE id)
{
 	if (PacketLossReason::PACKET_LOSS == id  ) { BRN_DEBUG("PACKET_LOSS_ROOT_NODE\n");}
	else  if (PacketLossReason::INTERFERENCE == id){ BRN_DEBUG("INTERFERENCE\n");}
	else  if (PacketLossReason::CHANNEL_FADING == id){BRN_DEBUG("CHANNEL_FADING\n");}
	else if (PacketLossReason::WIFI == id){BRN_DEBUG("WIFI\n");}
	else if (PacketLossReason::NON_WIFI == id){BRN_DEBUG("NON_WIFI\n");}
	else if (PacketLossReason::WEAK_SIGNAL == id){BRN_DEBUG("WEAK_SIGNAL\n");}
	else if (PacketLossReason::SHADOWING == id){BRN_DEBUG("SHADOWING\n");}
	else if (PacketLossReason::MULTIPATH_FADING == id){BRN_DEBUG("MULTIPATH_FADING\n");}
	else if (PacketLossReason::CO_CHANNEL == id){BRN_DEBUG("CO_CHANNEL\n");}
	else if (PacketLossReason::ADJACENT_CHANNEL == id){BRN_DEBUG("ADJACENT_CHANNEL\n");}
	else if (PacketLossReason::G_VS_B == id){BRN_DEBUG("G_VS_B\n");}
	else if (PacketLossReason::NARROWBAND == id){BRN_DEBUG("NARROWBAND\n");}
	else if (PacketLossReason::BROADBAND == id){BRN_DEBUG("BROADBAND\n");}
	else if (PacketLossReason::IN_RANGE == id){BRN_DEBUG("IN_RANGE\n");}
	else if (PacketLossReason::HIDDEN_NODE == id){BRN_DEBUG("HIDDEN_NODE\n");}
	else if (PacketLossReason::NARROWBAND_COOPERATIVE == id){BRN_DEBUG("NARROWBAND_COOPERATIVE\n");}
	else if (PacketLossReason::NARROWBAND_NON_COOPERATIVE == id){BRN_DEBUG("NARROWBAND_NON_COOPERATIVE\n");}
	else if (PacketLossReason::BROADBAND_COOPERATIVE == id){BRN_DEBUG("BROADBAND_COOPERATIVE\n");}
	else if (PacketLossReason::BROADBAND_NON_COOPERATIVE == id){BRN_DEBUG("BROADBAND_NON_COOPERATIVE\n");}
}

void PacketLossInformation::write_test_obj(PacketLossReason* ptr_obj)
{
	 PacketLossInformation::write_test_id(ptr_obj->getID());
	 BRN_DEBUG("Label = %d\n",ptr_obj->getLabel());
	 BRN_DEBUG("Parent = ");
	 PacketLossReason* ptr_parent = ptr_obj->getParent();
	 if (NULL != ptr_parent) PacketLossInformation::write_test_id(ptr_parent->getID());
	 else {
		BRN_DEBUG("No\n");
	}
	BRN_DEBUG("Fraction = %d\n",ptr_obj->getFraction());
	BRN_DEBUG("Statistics = %d\n",ptr_obj->getStatistics());
 	ptr_obj->write_test_childs();
} 

int PacketLossInformation::overall_fract(PacketLossReason* ptr_node,int depth)
{
	return ptr_node->overall_fract(depth);
}


PacketLossReason* PacketLossInformation::get_reason_by_id(PacketLossReason::PossibilityE id)
{
	return	map_poss_packetlossreson.get(id);
}

PacketLossReason::PossibilityE PacketLossInformation::get_possibility_by_name(String name)
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



PacketLossReason* PacketLossInformation::get_reason_by_name(String name)
{
	PacketLossReason::PossibilityE id =  get_possibility_by_name(name);
	return	get_reason_by_id(id);
}

PacketLossReason* PacketLossInformation::get_root_node()
{ 
	return _root;
}

void PacketLossInformation::test() 
{
	 BRN_DEBUG("PacketLossInformation::test() start\n");
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
	ptr_PACKET_LOSS_ROOT_NODE->setChild(1,ptr_CHANNEL_FADING);
	ptr_PACKET_LOSS_ROOT_NODE->setFraction(80);
	map_poss_packetlossreson.set(ptr_PACKET_LOSS_ROOT_NODE->getID(),ptr_PACKET_LOSS_ROOT_NODE);

	ptr_INTERFERENCE->setID(PacketLossReason::INTERFERENCE);
 	ptr_INTERFERENCE->setParent(ptr_PACKET_LOSS_ROOT_NODE);
 	ptr_INTERFERENCE->setChild(0,ptr_WIFI);
	ptr_INTERFERENCE->setChild(1,ptr_NON_WIFI);
	ptr_INTERFERENCE->setFraction(60);
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
	ptr_WIFI->setFraction(40);
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
	ptr_CO_CHANNEL->setFraction(20);
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
	ptr_BROADBAND->setFraction(5);
	map_poss_packetlossreson.set(ptr_BROADBAND->getID(),ptr_BROADBAND);

	ptr_IN_RANGE->setID(PacketLossReason::IN_RANGE);
 	ptr_IN_RANGE->setParent(ptr_CO_CHANNEL);
	map_poss_packetlossreson.set(ptr_IN_RANGE->getID(),ptr_IN_RANGE);

	ptr_HIDDEN_NODE->setID(PacketLossReason::HIDDEN_NODE);
 	ptr_HIDDEN_NODE->setParent(ptr_CO_CHANNEL);
	ptr_HIDDEN_NODE->setFraction(10);
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
	ptr_BROADBAND_NON_COOPERATIVE->setFraction(5);
	map_poss_packetlossreson.set(ptr_BROADBAND_NON_COOPERATIVE->getID(),ptr_BROADBAND_NON_COOPERATIVE);
	//debug output
	PacketLossInformation::write_test_obj(ptr_PACKET_LOSS_ROOT_NODE);
	PacketLossInformation::write_test_obj(ptr_INTERFERENCE);
	PacketLossInformation::write_test_obj(ptr_CHANNEL_FADING);
	PacketLossInformation::write_test_obj(ptr_WIFI);
	PacketLossInformation::write_test_obj(ptr_NON_WIFI);
	PacketLossInformation::write_test_obj(ptr_WEAK_SIGNAL);
	PacketLossInformation::write_test_obj(ptr_SHADOWING);
	PacketLossInformation::write_test_obj(ptr_MULTIPATH_FADING);
	PacketLossInformation::write_test_obj(ptr_CO_CHANNEL);
	PacketLossInformation::write_test_obj(ptr_ADJACENT_CHANNEL);
	PacketLossInformation::write_test_obj(ptr_G_VS_B);
	PacketLossInformation::write_test_obj(ptr_NARROWBAND);
	PacketLossInformation::write_test_obj(ptr_BROADBAND);
	PacketLossInformation::write_test_obj(ptr_IN_RANGE);
	PacketLossInformation::write_test_obj(ptr_HIDDEN_NODE);
	PacketLossInformation::write_test_obj(ptr_NARROWBAND_COOPERATIVE);
	PacketLossInformation::write_test_obj(ptr_NARROWBAND_NON_COOPERATIVE);
	PacketLossInformation::write_test_obj(ptr_BROADBAND_COOPERATIVE);
	PacketLossInformation::write_test_obj(ptr_BROADBAND_NON_COOPERATIVE);

      	BRN_DEBUG("PacketLossInformation::test() end\n");
      	BRN_DEBUG("Hidden-Node overallFkt = %d",overall_fract(ptr_HIDDEN_NODE,4));
	PacketLossReason *ptr_reason = get_reason_by_id(PacketLossReason::BROADBAND_NON_COOPERATIVE);
	BRN_DEBUG("REASON\n");
	if (NULL != ptr_reason ) { 
		BRN_DEBUG("Packet-Loss-Reason get-fkt()");  
		write_test_id(ptr_reason->getID());
		BRN_DEBUG("Pointer-Reason-Fraktion = %d",ptr_reason->getFraction());
	}
	else if(NULL == ptr_reason) BRN_DEBUG("PTR-REASON is NULL");
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(PacketLossReason)
EXPORT_ELEMENT(PacketLossInformation)
