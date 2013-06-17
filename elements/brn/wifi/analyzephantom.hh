#ifndef CLICK_ANALYZE_PHANTOM_HH
#define CLICK_ANALYZE_PHANTOM_HH

#include <click/element.hh>
#include <click/config.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <click/timer.hh>
#include <clicknet/wifi.h>

#include "elements/brn/wifi/brnwifi.hh"
#include "elements/brn/brnelement.hh"

#define INIT      -1
#define QUEUE_LEN  1

#define STATE_RX      1
#define STATE_SILENCE 2
#define STATE_STRANGE 3

#define RX_OK 0

/* phantom state ring buffer size */
#define PH_BUF_SIZE 16

/* max. time gap in usec allowed between 2 pkts to still count as consecutive pkts */
#define MAX_GAP 100

#define DEFAULT_DELAY 10
#define RESET 0
#define NEW_LAST_PACKET 1


CLICK_DECLS

class AnalyzePhantom : public BRNElement {

public:
  AnalyzePhantom();
  ~AnalyzePhantom();

  const char *class_name() const { return "AnalyzePhantom"; }
  const char *port_count() const { return PORTS_1_1; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &conf, ErrorHandler* errh);
  int initialize(ErrorHandler *);
  Packet *simple_action(Packet *p);

  void run_timer(Timer *);

  void analyze_last();
  int analyze_new(Packet *p);

  void delay_packet(Packet *p);
  void send_last_packet();

  u_int64_t get_packet_gap_via_timestamp(Packet *p); /* pkt gap in usec */
  u_int64_t get_packet_gap_via_hosttime(Packet *p);  /* pkt gap in usec */

  Packet *last_packet;
  Timer delay_timer;
  u_int16_t delay;
};


struct ph_state_buf_entry {
  u_int32_t prev_state;
  u_int32_t curr_state;

  u_int64_t change_time;
} __attribute__((packed));


/* additional phantom data which is supposed to be put into a phantom pkt */
struct add_phantom_data {
  u_int32_t endianness;
  u_int32_t version;
  u_int32_t rb_size;    /* both, flag and size. if 0 -> no ring buffer */

  struct ph_state_buf_entry ph_rb[PH_BUF_SIZE];  /* ph state ringbuffer */
  u_int32_t ph_rb_index;

  u_int64_t ph_start;   /* start and end time of a phantom pkt in 'k_time' */
  u_int64_t ph_stop;
  u_int64_t ph_len;     /* packet length/duration in ns */
  u_int32_t next_state; /* state after ph pkt push */
} __attribute__((packed));


CLICK_ENDDECLS

#endif /* CLICK_ANALYZE_PHANTOM_HH */
