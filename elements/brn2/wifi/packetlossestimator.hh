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
#include "elements/brn2/wifi/cooperativechannelstats.hh"
#include "elements/brn2/wifi/packetlossinformation/packetlossinformation.hh"
#include <click/ip6address.hh>
#include <elements/analysis/timesortedsched.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"

CLICK_DECLS

class PacketLossEstimator : public BRNElement {
    
    struct packetloss_statistics {

        uint8_t hidden_node;
        uint8_t inrange_coll;
        uint8_t non_wifi;
        uint8_t weak_signal;
    };

    
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
    class StatsCircularBuffer {
                
    private:
        uint32_t size;
        uint32_t start_elem;
        uint32_t counter;
        
        HashMap<EtherAddress, struct packetloss_statistics>* time_buffer;
        
    public:
        StatsCircularBuffer(uint32_t start_size) {
            
            start_elem = 0;
            counter = 0;
            size = start_size;
            time_buffer = new HashMap<EtherAddress, struct packetloss_statistics>[start_size];
        }
        
        ~StatsCircularBuffer() {
            
            delete[] time_buffer;
            start_elem = 0;
            counter = 0;
            size = 0;
        }
        
        inline uint32_t get_size() {
            return size;
        }
        
        inline bool is_full () {
            return size == counter;
        }
        
        inline bool is_empty () {
            return counter == 0;
        }
        
        void put_data_in (HashMap<EtherAddress, struct packetloss_statistics> *data) {
            
            uint32_t last_elem = (start_elem + counter) % size;
            time_buffer[last_elem] = *data;
            
            if (size == counter) {
                start_elem = (start_elem + 1) % size;   // if circular buffer filled completely override
            } else {
                start_elem++;
            }
        }
        
        bool pull_data (HashMap<EtherAddress, struct packetloss_statistics> *data) {
            
            *data = time_buffer[start_elem];
            start_elem = (start_elem + 1) % size;
            counter--;
            return true;
        }
        
        uint32_t read_data (HashMap<EtherAddress, struct packetloss_statistics> *data, uint32_t entries) {
            
            if (entries > size) {
                return 0;
            }
            
            HashMap<EtherAddress, struct packetloss_statistics> temp_data[entries];
            uint32_t overflow_counter = size;
            uint32_t i = 0;
            
            for (i; i < entries; i++) {
                
                if (start_elem - i < 0) {
                    
                    if (NULL == &time_buffer[overflow_counter]) {
                        
                        break;
                    }
                    
                    temp_data[i] = time_buffer[overflow_counter--];
                    
                } else {
                    temp_data[i] = time_buffer[start_elem - i];
                }
            }
            
            *data = *temp_data;
            return i;
        }
    };
//public:

    PacketLossEstimator();
    ~PacketLossEstimator();

    const char *class_name() const { return "PacketLossEstimator"; }

    const char *port_count() const { return PORTS_1_1; }

    const char *processing() const { return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    
    Packet *simple_action(Packet *);
    
    void add_handlers();
    
    String stats_handler(int);
    
    void add_ack(EtherAddress);
    
    uint32_t get_acks_by_node(EtherAddress);

private:
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
    /// ACKS received from other nodes
    static HashMap<EtherAddress, uint32_t> _acks_by_node;
    
    static StatsCircularBuffer _stats_buffer;
    /// switch if pessimistic hidden node prediction is used
    bool _pessimistic_hn_detection;
    /// Device pointer
    BRN2Device *_dev;
    /// Structure for gathering information about current packet
    PacketParameter *_packet_parameter;
    
    ///< Estimate probability of channel error because of hidden nodes
    void estimateHiddenNode(struct packetloss_statistics *);
    ///< Estimate probability of channel error because of inrange collisions
    void estimateInrange(struct packetloss_statistics *);
    ///< Estimate probability of channel error because non-wifi-signals
    void estimateNonWifi(struct airtime_stats *, struct packetloss_statistics *);
    ///< Estimate probability of channel error because of weak signal
    void estimateWeakSignal(ChannelStats::SrcInfo *, struct packetloss_statistics *);
    ///< put all necessary information about the current packet into one structure
    void gather_packet_infos_(Packet *);
    
    StringAccum stats_get_hidden_node(HiddenNodeDetection::NodeInfoTable *, PacketLossInformation *);
    StringAccum stats_get_inrange(HiddenNodeDetection::NodeInfoTable *, PacketLossInformation *);
    StringAccum stats_get_weak_signal(HiddenNodeDetection::NodeInfoTable *, PacketLossInformation *);
    StringAccum stats_get_non_wifi(HiddenNodeDetection::NodeInfoTable *, PacketLossInformation *);
};

CLICK_ENDDECLS
#endif
