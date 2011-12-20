/*
 * Node for Packetlossreason
 */

#include <click/config.h>
#include "packetlossreason.hh"

CLICK_DECLS

/*
 * Konstruktor und Destruktor
*/

PacketLossReason::PacketLossReason()
{
	ptr_parent = NULL;
	fraction = 0;
	stats = NULL;
	possiblity_id = PACKET_LOSS_ROOT_NODE;
	label = 0;
}

PacketLossReason::~PacketLossReason()
{
}

<<<<<<< HEAD
=======

int
PacketLossReason::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret = cp_va_kparse(conf, this, errh,
                         "DEBUG", cpkP, cpInteger, &_debug,
                         cpEnd);

  return ret;
}

PacketLossReason::Packet_Loss_Reason id;//fraction type

int label;//node label

PacketLossReason *ptr_parent;//parent node

//HashTable<String, int> h(0);
//map<int, PacketLossReason> childs;//child nodes


int fraction; // 0 -100
void *stats;//additional statics
>>>>>>> bc27073c936a8a884383c0437a803e245dd89d3c

//calculation of likelihood
int PacketLossReason::overall_fract(int depth)
{
	if (ptr_parent == NULL) return fraction;
	return	fraction * ptr_parent -> overall_fract(depth-1);
//	PacketLossReason *ptr_pa = get

//	int fraction = 1;
//	for (int i = 0; i < depth;i++) {
//		if (ptr_parent == NULL) return fraction;
//		fraction = fraction * ptr_parent
//	}
} 

<<<<<<< HEAD
PacketLossReason::PossibilityE PacketLossReason::write_test_id(int id)
{
	if (0 == id  ) { click_chatter("PACKET_LOSS_ROOT_NODE");}
	else  if (1 == id){ click_chatter("INTERFERENCE");}
	else  if (2 == id){click_chatter("CHANNEL_FADING");}
	else if (3 == id){click_chatter("WIFI");}
	else if (4 == id){click_chatter("NON_WIFI");}
	else if (5 == id){click_chatter("WEAK_SIGNAL");}
	else if (6 == id){click_chatter("SHADOWING");}
	else if (7 == id){click_chatter("MULTIPATH_FADING");}
	else if (8 == id){click_chatter("CO_CHANNEL");}
	else if (9 == id){click_chatter("ADJACENT_CHANNEL");}
	else if (10 == id){click_chatter("G_VS_B");}
	else if (11 == id){click_chatter("NARROWBAND");}
	else if (12 == id){click_chatter("BROADBAND");}
	else if (13 == id){click_chatter("IN_RANGE");}
	else if (14 == id){click_chatter("HIDDEN_NODE");}
	else if (15 == id){click_chatter("NARROWBAND_COOPERATIVE");}
	else if (16 == id){click_chatter("NARROWBAND_NON_COOPERATIVE");}
	else if (17 == id){click_chatter("BROADBAND_COOPERATIVE");}
	else if (18 == id){click_chatter("BROADBAND_NON_COOPERATIVE");}
}

void PacketLossReason::write_test_childs()
=======
void PacketLossReason::test()
{/*
	 h["A"] = 1;
  if (!h["B"])      // Nota bene
      printf("B wasn't in table, but it is now\n");
  for (HashMap<String, int>::iterator it = h.begin(); it; ++it)
      printf("%s -> %d\n", it.key().c_str(), it.value());
                    // Prints  B wasn't in table, but it is now
                    //         A -> 1
                    //         B -> 0
  */
}
static void graph_packet_loss_init()
>>>>>>> bc27073c936a8a884383c0437a803e245dd89d3c
{
	for (HashTable<int,PacketLossReason*>::iterator it = children.begin(); it; ++it) {
		PacketLossReason::write_test_id(getID());
		click_chatter("->");
		PacketLossReason::PossibilityE child = PacketLossReason::write_test_id((it.value())->getID()); 
		click_chatter("\n");
	}
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

