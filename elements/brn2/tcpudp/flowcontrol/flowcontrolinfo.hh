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
    ~FlowControlInfo();

    Packet *packet_window;
    uint16_t max_window_size;
    uint16_t cur_window_size;
    uint16_t window_index;
    uint16_t min_seq;
   
    uint16_t next_seq;

    uint32_t rtt;

    uint16_t frag_size;

    void insert_packet(Packet */*p*/, uint16_t /*seq*/) {
      return;
    }

    uint32_t non_acked_packets() {
      return 0;
    }

    void clear() {
    }

};

CLICK_ENDDECLS
#endif
