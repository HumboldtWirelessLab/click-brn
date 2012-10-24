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
    FlowControlInfo(uint16_t flowid);
    FlowControlInfo(uint16_t flowid, uint16_t max_window_size);
    ~FlowControlInfo();

    Packet **_packet_window;
    bool *_ack_window;
    Timestamp *_packet_times;

    uint16_t _flowid;

    uint16_t _max_window_size;
    uint16_t _cur_window_size;

    uint16_t _window_start_index;
    uint16_t _window_next_index;

    uint16_t _min_seq;
    uint16_t _next_seq;

    uint32_t _rtt;

    int insert_packet(Packet *p);

    bool has_packet(uint16_t seq);
    int insert_packet(Packet *p, uint16_t seq);

    bool packet_fits_in_window(uint16_t seq);
    Packet* get_and_clear_acked_seq();

    int32_t max_not_acked_seq();

    void clear_packets_up_to_seq(uint16_t seq); //source

    void clear();

    int32_t min_age_not_acked();
    Packet *get_packet_with_max_age(int32_t age);

    String print_info();
};

typedef HashMap<uint16_t, FlowControlInfo*> FlowTable;
typedef FlowTable::const_iterator FTIter;

CLICK_ENDDECLS
#endif
