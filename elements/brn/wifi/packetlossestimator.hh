#ifndef CLICK_PACKETLOSSESTIMATOR_HH
#define CLICK_PACKETLOSSESTIMATOR_HH

#include <math.h>
#include <click/config.h>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/ip6address.hh>
#include <click/packet_anno.hh>
#if CLICK_NS
	#include <click/router.hh>
#endif

#include "elements/analysis/timesortedsched.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/wifi/channelstats.hh"
#include "elements/brn/wifi/collisioninfo.hh"
#include "elements/brn/wifi/hiddennodedetection.hh"
#include "elements/brn/wifi/cooperativechannelstats.hh"
#include "elements/brn/wifi/packetlossinformation/packetlossinformation.hh"
#include "elements/brn/wifi/packetlossestimationhelper/statscircularbuffer.hh"
#include "elements/brn/wifi/packetlossestimationhelper/packetparameter.hh"

CLICK_DECLS

class PacketLossEstimator: public BRNElement
{

public:

    PacketLossEstimator();
    ~PacketLossEstimator();

    const char *class_name() const { return "PacketLossEstimator"; }
    const char *port_count() const { return PORTS_1_1; }
    const char *processing() const { return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    Packet *simple_action(Packet *);
    void add_handlers();
    String stats_handler(int);
    static uint8_t _max_weak_signal_value;

private:
    /// ACKS received from other nodes
    static HashMap<EtherAddress, uint32_t> _acks_by_node;
    /// Addresses packets received from
    static Vector<EtherAddress> _received_adrs;
    /// ACKS received from other nodes last period (mostly 1 s)
//    static HashMap<EtherAddress, uint32_t> _last_acks_by_node;
    /// Buffer for mid and long term statistics
    static StatsCircularBuffer _stats_buffer;
    ///
    static HashMap<uint8_t, HashMap<uint8_t, uint64_t> > _rssi_histogram;
    /// Collected Channel Stats
    ChannelStats *_cst;
    /// Collected Collision Infos
    CollisionInfo *_cinfo;
    /// Collected Hidden Node Detection Stats
    HiddenNodeDetection *_hnd;
    /// Class for storing statistical data about lost packets
    PacketLossInformation *_pli;
    /// Statistics from cooperating nodes
    CooperativeChannelStats *_cocst;
    /// Device pointer
    BRN2Device *_dev;
    /// switch if pessimistic hidden node prediction is used
    bool _pessimistic_hn_detection;
    /// Structure for gathering information about current packet
    PacketParameter *_packet_parameter;

    ///< Estimate probability of channel error because of hidden nodes
    void estimateHiddenNode();
    ///< Estimate probability of channel error because of inrange collisions
    void estimateInrange();
    ///< Estimate probability of channel error because non-wifi-signals
    void estimateNonWifi(struct airtime_stats &);
    ///< Estimate probability of channel error because of weak signal
    void estimateWeakSignal(ChannelStats::SrcInfo *, ChannelStats::RSSIInfo &);
    ///< Put all necessary information about the current packet into one structure
    void gather_packet_infos_(const Packet &);
    ///< Add received ACK-Packet to _acks_by_node-Hashmap
    void add_ack(const EtherAddress &);
    ///< Get number of received ACK-Packets for an ether address
    uint32_t get_acks_by_node(const EtherAddress &);
    void reset_acks();
    ///<
    uint8_t calc_weak_signal_percentage(ChannelStats::SrcInfo *, ChannelStats::RSSIInfo &);
    ///<
    void update_statistics(HiddenNodeDetection::NodeInfoTable &, PacketLossInformation &);

    StringAccum stats_get_hidden_node(HiddenNodeDetection::NodeInfoTable &, PacketLossInformation &);
    StringAccum stats_get_inrange(HiddenNodeDetection::NodeInfoTable &, PacketLossInformation &);
    StringAccum stats_get_weak_signal(HiddenNodeDetection::NodeInfoTable &, PacketLossInformation &);
    StringAccum stats_get_non_wifi(HiddenNodeDetection::NodeInfoTable &, PacketLossInformation &);
};

CLICK_ENDDECLS
#endif
