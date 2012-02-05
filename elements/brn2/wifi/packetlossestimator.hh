#ifndef CLICK_PACKETLOSSESTIMATOR_HH
#define CLICK_PACKETLOSSESTIMATOR_HH

#include <click/element.hh>
#include <click/vector.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/wifi/channelstats.hh"
#include "elements/brn2/wifi/collisioninfo.hh"
#include "elements/brn2/wifi/hiddennodedetection.hh"
#include "elements/brn2/wifi/packetlossinformation/packetlossinformation.hh"

CLICK_DECLS

class PacketLossEstimator : public BRNElement {
  
  class Node {
    uint64_t packets;
    Timestamp last_packet;
    Timestamp last10_packets;
    Timestamp last100_packets;
    Timestamp last1000_packets;
    
  public:
    inline void update() {
      packets++;
      last_packet = Timestamp::now();
      if(packets % 10 == 0) {
	last10_packets = last_packet - last10_packets;
        if(packets % 100 == 0) {
	  last100_packets = last_packet - last100_packets;
          if(packets % 1000 == 0) {
	    last1000_packets = last_packet - last1000_packets;
	  }
	}
      }
    }
    
    inline uint32_t getcurTraffic() {
      
      return 100 - last10_packets.msec()/10;      
    }
    
    Node() {
      packets = 1;
      last_packet = Timestamp::now();
      last10_packets = Timestamp();
      last100_packets = Timestamp();
      last1000_packets = Timestamp();
    }
  };
	
  class NodeList : public BRNElement{
    private:
    	HashMap<EtherAddress, HashMap<EtherAddress, Node*> > nodelist;
        
    public:
	NodeList() {
	  _debug = 4;	
	}
    const char *class_name() const  { return "PacketLossEstimator"; }
    const char *port_count() const { return PORTS_1_1; }
    const char *processing() const { return AGNOSTIC; }


    inline bool contains(EtherAddress srcAddress) {
      
      return (nodelist.find_pair(srcAddress) != 0);      
    }
  
    inline void updateNeighbours(EtherAddress srcAddress, EtherAddress dstAddress) {
	
	Node *node = nodelist.find(srcAddress).find(dstAddress);
	if (node) {
	  BRN_DEBUG("Node found, update Datatransfer: %s", dstAddress.unparse().c_str());
	  node->update();
	} else {
	  BRN_DEBUG("No Node found, inserting new one: %s", dstAddress.unparse().c_str());
	  HashMap<EtherAddress, Node*> temp = nodelist.find(srcAddress);
	  temp.insert(dstAddress, new Node());
	  nodelist.insert(srcAddress, temp);
	}
	//speichere zur dst neuen datentransfer, wenn dst noch nicht vorhanden, dann füge ein
    }
   
    inline void add(EtherAddress srcAddress, EtherAddress dstAddress) {
      BRN_DEBUG("Add new node: %s with connection to %s", srcAddress.unparse().c_str(), dstAddress.unparse().c_str());
      HashMap<EtherAddress, Node*> temp;
      temp.insert(dstAddress, new Node());
      nodelist.insert(srcAddress, temp);
      //füge neue src address mit dstadresse ein      
    }
    
    inline uint8_t calcHiddenNodesProbability(EtherAddress ownAddress, EtherAddress srcAddress) {
      
      HashMap<EtherAddress, Node*> ownAddressMap = nodelist.find(ownAddress);
      HashMap<EtherAddress, Node*> neighbour = nodelist.find(srcAddress);
      int hnProbability = 0;
	for(HashMap<EtherAddress, Node*>::iterator i = ownAddressMap.begin();
	    i != ownAddressMap.end();
	    i++) {
	  EtherAddress tmpAdd = i.key();
	  BRN_DEBUG("%s own Neighbour AddressMap: %s", ownAddress.unparse().c_str(), tmpAdd.unparse().c_str());
	}
      for(HashMap<EtherAddress, Node*>::iterator i = neighbour.begin();
	    i != neighbour.end();
	    i++) {
	EtherAddress tmpAdd = i.key();
	BRN_DEBUG("NeighbourAddress: %s", tmpAdd.unparse().c_str());
	if(!ownAddressMap.find(tmpAdd)) {
	  BRN_DEBUG("Not in my Neighbour List");
	  hnProbability += i.value()->getcurTraffic();
	} else {
	  BRN_DEBUG("In my Neighbour List");
	}
      }

      return hnProbability;
	//finde alle Nachbarn von srcAddress, die kein Nachbar von ownAddress sind und gewichte wahrscheinlichkeiten nach datentransfer
    }
  };
  
  public:
    PacketLossEstimator();
    ~PacketLossEstimator();

    const char *class_name() const  { return "PacketLossEstimator"; }
    const char *port_count() const { return PORTS_1_1; }
    const char *processing() const { return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    Packet *simple_action(Packet *);

    void add_handlers();
    String stats_handler(int);
    void addAddress2NodeList(EtherAddress, EtherAddress);
  private:
     /// Collected Channel Stats
    ChannelStats *_cst;
    /// Collected Collision Infos
    CollisionInfo *_cinfo;
    /// Collected Hidden Node Detection Stats
    HiddenNodeDetection *_hnd;
    /// Class for storing statistical data about lost packets
    PacketLossInformation *_pli;
  
    BRN2Device *_dev;
 
    NodeList nodeList;
  
    ///< add new node to PLI. Returns 0 if node inserted, 1 if node already exists, >0 in error case.
    int8_t addNewPLINode(EtherAddress);
    ///< Retrieve Source-Mac-Address from packet.
    EtherAddress getSrcAddress(Packet *);
    
    uint8_t getPacketType(Packet *);
    ///< Adopt the statistics for an adress according to new collected datas
    void updatePacketlossStatistics(EtherAddress, EtherAddress, int, int[]);
  
    void estimateHiddenNode(EtherAddress, EtherAddress, int);
    void estimateInrange(EtherAddress, int[]);
    void estimateNonWifi(int[]);
};

CLICK_ENDDECLS
#endif
