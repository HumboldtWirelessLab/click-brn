#ifndef CLICK_PACKETLOSSESTIMATOR_HH
#define CLICK_PACKETLOSSESTIMATOR_HH

#include <click/config.h>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/ip6address.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/wifi/rxinfo/channelstats/channelstats.hh"
#include "elements/brn/wifi/rxinfo/channelstats/cooperativechannelstats.hh"

#include "elements/brn/wifi/rxinfo/collisioninfo.hh"
#include "elements/brn/wifi/rxinfo/hiddennodedetection.hh"

#include "packetlossinformation/packetlossinformation.hh"

CLICK_DECLS

class PacketLossEstimator: public BRNElement
{

  public:

    class PacketLossStatistics {
     public:
      int timestamp;
      uint8_t inrange_prob;
      uint8_t hidden_node_prob;
      uint8_t weak_signal_prob;
      uint8_t non_wifi_prob;
    };

    typedef HashMap<EtherAddress, Vector<PacketLossStatistics>*> PacketLossStatisticsMap;
    typedef PacketLossStatisticsMap::const_iterator PacketLossStatisticsMapIter;


    PacketLossEstimator();
    ~PacketLossEstimator();

    const char *class_name() const { return "PacketLossEstimator"; }
    const char *port_count() const { return PORTS_1_1; }
    const char *processing() const { return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);

    Packet *simple_action(Packet *);
    void add_handlers();

    String stats_handler(int);

    uint8_t _max_weak_signal_value;

private:

    /// Device pointer
    BRN2Device *_dev;
    /// Collected Channel Stats
    ChannelStats *_cst;
    /// Statistics from cooperating nodes
    CooperativeChannelStats *_cocst;
    /// Collected Collision Infos
    CollisionInfo *_cinfo;
    /// Collected Hidden Node Detection Stats
    HiddenNodeDetection *_hnd;

    /// Class for storing statistical data about lost packets
    PacketLossInformation *_pli;

    /// Addresses packets received from
    HashMap<EtherAddress,Timestamp> _received_adrs;

    ///
    HashMap<uint8_t, HashMap<uint8_t, uint64_t> > _rssi_histogram;

    PacketLossStatisticsMap _stats_buffer;

    /// switch if pessimistic hidden node prediction is used
    bool _pessimistic_hn_detection;

    ///< Estimate probability of channel error because of hidden nodes
    void estimateHiddenNode(EtherAddress &src, EtherAddress &dst);
    ///< Estimate probability of channel error because of inrange collisions
    void estimateInrange(EtherAddress &src, EtherAddress &dst);
    ///< Estimate probability of channel error because non-wifi-signals
    void estimateNonWifi(EtherAddress &src, EtherAddress &dst, struct airtime_stats *);
    ///< Estimate probability of channel error because of weak signal
    void estimateWeakSignal(EtherAddress &src, EtherAddress &dst, ChannelStats::SrcInfo *, ChannelStats::RSSIInfo *);

    ///<
    uint8_t calc_weak_signal_percentage(ChannelStats::SrcInfo *, ChannelStats::RSSIInfo *);
    ///<
    void update_statistics(HiddenNodeDetection::NodeInfoTable *, PacketLossInformation *);

    StringAccum stats_get_hidden_node(HiddenNodeDetection::NodeInfoTable *, PacketLossInformation *);
    StringAccum stats_get_inrange(HiddenNodeDetection::NodeInfoTable *, PacketLossInformation *);
    StringAccum stats_get_weak_signal(HiddenNodeDetection::NodeInfoTable *, PacketLossInformation *);
    StringAccum stats_get_non_wifi(HiddenNodeDetection::NodeInfoTable *, PacketLossInformation *);
};

CLICK_ENDDECLS
#endif
