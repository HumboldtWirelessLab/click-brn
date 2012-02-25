/*
 * Node for Packetlossreason
 */

#include <click/config.h>
#include <click/straccum.hh>
#include "packetlossreason.hh"



CLICK_DECLS

#undef DEBUG


/*
 * Konstruktor und Destruktor
*/

PacketLossReason::PacketLossReason()
{
	ptr_parent = NULL;
	fraction = 0;
	stats = NULL;
	possiblity_id = PACKET_LOSS;
	label = 0;
}

PacketLossReason::~PacketLossReason()
{
}


//calculation of likelihood
int PacketLossReason::overall_fract(int depth)
{
	if (ptr_parent == NULL) return fraction;
	return	fraction * ptr_parent -> overall_fract(depth-1);
} 


PacketLossReason::PossibilityE PacketLossReason::id_get(unsigned int id) 
{
	if (0 == id) { return PACKET_LOSS;}
	else  if (1 == id){ return INTERFERENCE;}
	else  if (2 == id){ return CHANNEL_FADING;}
	else if (3 == id){return WIFI;}
	else if (4 == id){return NON_WIFI;}
	else if (5 == id){return WEAK_SIGNAL;}
	else if (6 == id){return SHADOWING;}
	else if (7 == id){return MULTIPATH_FADING;}
	else if (8 == id){return CO_CHANNEL;}
	else if (9 == id){return ADJACENT_CHANNEL;}
	else if (10 == id){return G_VS_B;}
	else if (11 == id){return NARROWBAND;}
	else if (12 == id){return BROADBAND;}
	else if (13 == id){return IN_RANGE;}
	else if (14 == id){return HIDDEN_NODE;}
	else if (15 == id){return NARROWBAND_COOPERATIVE;}
	else if (16 == id){return NARROWBAND_NON_COOPERATIVE;}
	else if (17 == id){return BROADBAND_COOPERATIVE;}
	else if (18 == id){return BROADBAND_NON_COOPERATIVE;}
}


String PacketLossReason::print() 
{
	StringAccum sa;
	int id = getID();
	sa << "<packetlossreason reason=\"";
	if (0 == id) { sa << "PACKET_LOSS_ROOT_NODE";}
	else  if (1 == id){ sa <<"INTERFERENCE";}
	else  if (2 == id){sa <<"CHANNEL_FADING";}
	else if (3 == id){sa <<"WIFI";}
	else if (4 == id){sa <<"NON_WIFI";}
	else if (5 == id){sa <<"WEAK_SIGNAL";}
	else if (6 == id){sa <<"SHADOWING";}
	else if (7 == id){sa <<"MULTIPATH_FADING";}
	else if (8 == id){sa <<"CO_CHANNEL";}
	else if (9 == id){sa <<"ADJACENT_CHANNEL";}
	else if (10 == id){sa <<"G_VS_B";}
	else if (11 == id){sa <<"NARROWBAND";}
	else if (12 == id){sa <<"BROADBAND";}
	else if (13 == id){sa <<"IN_RANGE";}
	else if (14 == id){sa <<"HIDDEN_NODE";}
	else if (15 == id){sa <<"NARROWBAND_COOPERATIVE";}
	else if (16 == id){sa <<"NARROWBAND_NON_COOPERATIVE";}
	else if (17 == id){sa <<"BROADBAND_COOPERATIVE";}
	else if (18 == id){sa <<"BROADBAND_NON_COOPERATIVE";}
	sa << "\" label=\"" << getLabel() << "\" fraction=\"" <<  getFraction() << "\"/>";//</fraction>\n</packetlossreason>";
	return sa.take_string();
}


int PacketLossReason::getLabel() {
	return label; 
}


void PacketLossReason::setLabel(int la)
{
	label = la;
}

void PacketLossReason::setParent(PacketLossReason *ptr_paren){ 
	ptr_parent = ptr_paren;
}

PacketLossReason* PacketLossReason::getParent(){
	return ptr_parent;
}

void PacketLossReason::setChild(int poss, PacketLossReason* ptr_element_next)
{
	children[poss] = ptr_element_next;
}

PacketLossReason* PacketLossReason::getChild(PossibilityE poss) {
 	return  children.get(poss);
}


void PacketLossReason::setFraction( int frac) {
	if (frac >= 0 && frac <= 100) fraction = frac;
}
int PacketLossReason::getFraction()//0-100
{
	return fraction;
}

void PacketLossReason::setStatistics(void *st)
{
	stats = st;
}

void* PacketLossReason::getStatistics()
{
	return stats;
}

void PacketLossReason::setID(PossibilityE poss)
{
	possiblity_id = poss;
}

PacketLossReason::PossibilityE PacketLossReason::getID()
{
	return possiblity_id;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(PacketLossReason)

