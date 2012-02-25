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
#include <click/ip6address.hh>
#include <elements/analysis/timesortedsched.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"

CLICK_DECLS

struct packloss_corporation {
    
    EtherAddress neighbours[];
    uint8_t rssi; 
};

class PacketLossEstimator : public BRNElement {

    class PacketParameter {
        
    private:
        EtherAddress own_address;
        EtherAddress src_address;
        EtherAddress dst_address;
        uint8_t packet_type;

    public:

        inline EtherAddress get_own_address() {
            return own_address;
        }

        inline EtherAddress get_src_address() {
            return src_address;
        }

        inline EtherAddress get_dst_address() {
            return dst_address;
        }

        inline uint8_t get_packet_type() {
            return packet_type;
        }

        inline void put_params_(EtherAddress own, EtherAddress src, EtherAddress dst, uint8_t p_type) {

            own_address = own;
            src_address = src;
            dst_address = dst;
            packet_type = p_type;
        }
    };

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

private:
    /// Collected Channel Stats
    ChannelStats *_cst;
    /// Collected Collision Infos
    CollisionInfo *_cinfo;
    /// Collected Hidden Node Detection Stats
    HiddenNodeDetection *_hnd;
    /// Class for storing statistical data about lost packets
    PacketLossInformation *_pli;
    /// Device pointer
    BRN2Device *_dev;
    /// Switch cooperation on or off
    int *_coop;
    /// Structure for gathering information about current packet
    PacketParameter *packet_parameter;
    
    ///< Adopt the statistics for an adress according to new collected datas
    void updatePacketlossStatistics();
    ///< Estimate probability of channel error because of hidden nodes
    void estimateHiddenNode();
    ///< Estimate probability of channel error because of inrange collisions
    void estimateInrange();
    ///< Estimate probability of channel error because non-wifi-signals
    void estimateNonWifi();
    ///< Estimate probability of channel error because of weak signal
    void estimateWeakSignal(struct airtime_stats *);
    ///< put all necessary information about the current packet into one structure
    void gather_packet_infos_(Packet *);
};

CLICK_ENDDECLS
#endif
