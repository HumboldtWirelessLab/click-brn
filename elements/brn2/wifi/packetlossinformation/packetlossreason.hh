#ifndef CLICK_PacketLossReason_HH
#define CLICK_PacketLossReason_HH
#include <click/element.hh>
#include <click/hashtable.hh>


CLICK_DECLS

class PacketLossReason { 
public:

	PacketLossReason();
	~PacketLossReason();

	typedef enum _PacketLossReason {
	  PACKET_LOSS_ROOT_NODE = 0,
	  INTERFERENCE, 
	  CHANNEL_FADING,
	  WIFI,
	  NON_WIFI,
	  WEAK_SIGNAL,
	  SHADOWING,
	  MULTIPATH_FADING,
	  CO_CHANNEL,
	  ADJACENT_CHANNEL,
	  G_VS_B,
	  NARROWBAND,
	  BROADBAND,
	  IN_RANGE,
	  HIDDEN_NODE,
	  NARROWBAND_COOPERATIVE,
	  NARROWBAND_NON_COOPERATIVE,
	  BROADBAND_COOPERATIVE,
	  BROADBAND_NON_COOPERATIVE
	} PossibilityE;


	int overall_fract(int depth);
	// 0 = label off
	// 1 = label on
	int getLabel();
	void setLabel(int la);

	void setParent(PacketLossReason *ptr_paren);
	PacketLossReason* getParent();

	void setChild(int poss, PacketLossReason* ptr_element_next);

	PacketLossReason* getChild(PossibilityE poss);

	void write_test_childs();

	//Fraction range [0-100]
	void setFraction(int frac);
	int getFraction();

	//additional statics
	void setStatistics(void *st);
	void* getStatistics();

	void setID(PossibilityE poss);
	PossibilityE getID();

	PossibilityE write_test_id(int id);

private:
	PossibilityE possiblity_id;
	HashTable<int,PacketLossReason*> children;
	int label;
	PacketLossReason *ptr_parent;
	int fraction; 
	void *stats;

};

CLICK_ENDDECLS
#endif
