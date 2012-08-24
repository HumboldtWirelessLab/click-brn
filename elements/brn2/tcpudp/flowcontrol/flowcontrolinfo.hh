#ifndef FLOWCONTROLINFO_HH
#define FLOWCONTROLINFO_HH
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>

CLICK_DECLS

#define DEFAULT_MAX_WINDOW_SIZE 16


class FlowControlInfo {
  
  public:

    FlowControlInfo();
    FlowControlInfo(uint16_t max_window_size);
    ~FlowControlInfo();

    Packet **_packet_window;
    bool *_ack_window;
    
    uint16_t _max_window_size;
    uint16_t _cur_window_size;
    
    uint16_t _window_start_index;
    uint16_t _window_next_index;
    
    uint16_t _min_seq;
    uint16_t _next_seq;

    uint32_t _rtt;

    int insert_packet(Packet *p, uint16_t seq);
    
    uint32_t non_acked_packets();

    void clear();

};

CLICK_ENDDECLS
#endif
