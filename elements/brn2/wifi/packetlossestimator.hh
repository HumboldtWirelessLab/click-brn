#ifndef CLICK_PACKETLOSSESTIMATOR_HH
#define CLICK_PACKETLOSSESTIMATOR_HH

#include <click/element.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/wifi/channelstats.hh"
#include "elements/brn2/wifi/collisioninfo.hh"
#include "elements/brn2/wifi/hiddennodedetection.hh"
#include "elements/brn2/wifi/packetlossinformation/packetlossinformation.hh"

CLICK_DECLS

class PacketLossEstimator : public BRNElement {
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
    /// Debug switch
    uint8_t _debug;
    ///< add new node to PLI. Returns 0 if node inserted, 1 if node already exists, >0 in error case.
    int8_t addNewPLINode(EtherAddress);
    ///< Retrieve Source-Mac-Address from packet.
    EtherAddress getSrcAddress(Packet *);
    ///< Adopt the statistics for an adress according to new collected datas
    void updatePacketlossStatistics(EtherAddress, int[]);
  
    void estimateHiddenNode(EtherAddress);
    void estimateInrange(EtherAddress, int[]);
    void estimateNonWifi(int[]);
};

CLICK_ENDDECLS
#endif
