#ifndef CLICK_PACKETLOSSESTIMATOR_HH
#define CLICK_PACKETLOSSESTIMATOR_HH

#include <math.h>
#include <click/config.h>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/ip6address.hh>
#if CLICK_NS 
    #include <click/router.hh>
#endif

#include "elements/analysis/timesortedsched.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/wifi/channelstats.hh"
#include "elements/brn2/wifi/collisioninfo.hh"
#include "elements/brn2/wifi/hiddennodedetection.hh"
#include "elements/brn2/wifi/cooperativechannelstats.hh"
#include "elements/brn2/wifi/packetlossinformation/packetlossinformation.hh"
#include "elements/brn2/wifi/packetlossestimationhelper/statscircularbuffer.hh"
#include "elements/brn2/wifi/packetlossestimationhelper/packetparameter.hh"

CLICK_DECLS

class PacketLossEstimator : public BRNElement {
   
public:

    PacketLossEstimator();
    virtual ~PacketLossEstimator();

    const char *class_name() const { return "PacketLossEstimator"; }
    const char *port_count() const { return PORTS_1_1; }
    const char *processing() const { return AGNOSTIC; }
    int configure(Vector<String> &, ErrorHandler *);
    Packet *simple_action(Packet *);
    void add_handlers();
    String stats_handler(int);
    void add_ack(const EtherAddress &);
    uint32_t get_acks_by_node(const EtherAddress &);

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
    void estimateHiddenNode();
    ///< Estimate probability of channel error because of inrange collisions
    void estimateInrange();
    ///< Estimate probability of channel error because non-wifi-signals
    void estimateNonWifi(struct airtime_stats &);
    ///< Estimate probability of channel error because of weak signal
    void estimateWeakSignal(ChannelStats::SrcInfo &);
    ///< put all necessary information about the current packet into one structure
    void gather_packet_infos_(const Packet &);
    
    StringAccum stats_get_hidden_node(HiddenNodeDetection::NodeInfoTable &, PacketLossInformation &);
    StringAccum stats_get_inrange(HiddenNodeDetection::NodeInfoTable &, PacketLossInformation &);
    StringAccum stats_get_weak_signal(HiddenNodeDetection::NodeInfoTable &, PacketLossInformation &);
    StringAccum stats_get_non_wifi(HiddenNodeDetection::NodeInfoTable &, PacketLossInformation &);
};

CLICK_ENDDECLS
#endif

            /*
    class StatsCircularBuffer_old {
                
    private:
        uint32_t size;
        uint32_t start_elem;
        uint32_t counter;
        
        HashMap<EtherAddress, struct packetloss_statistics_old>* time_buffer;
        
    public:
        StatsCircularBuffer_old(uint32_t start_size) {
            
            start_elem = 0;
            counter = 0;
            size = start_size;
            time_buffer = new HashMap<EtherAddress, struct packetloss_statistics_old>[start_size];
        }
        
        ~StatsCircularBuffer_old() {
            
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
        
         void put_data_in (HashMap<EtherAddress, struct packetloss_statistics_old> *data) {
            
            uint32_t last_elem = (start_elem + counter) % size;
            time_buffer[last_elem] = *data;
            
            if (size == counter) {
                start_elem = (start_elem + 1) % size;   // if circular buffer filled completely override
            } else {
                start_elem++;
            }
        }
        
        bool pull_data (HashMap<EtherAddress, struct packetloss_statistics_old> *data) {
            
            *data = time_buffer[start_elem];
            start_elem = (start_elem + 1) % size;
            counter--;
            return true;
        }
        
        uint32_t read_data (HashMap<EtherAddress, struct packetloss_statistics_old> *data, uint32_t entries) {
            
            if (entries > size) {
                return 0;
            }
            
            HashMap<EtherAddress, struct packetloss_statistics_old> temp_data[entries];
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
           */ 
