#ifndef BRN2PACKETQUEUECONTROLELEMENT_HH
#define BRN2PACKETQUEUECONTROLELEMENT_HH

#include <click/element.hh>
#include <click/timer.hh>
#include <click/handlercall.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

/*
 * =c
 * BRN2PacketQueueControl()
 * =s
 * SEND Packet with given size
 * =d
 */

#define DEFAULT_PACKETHEADERSIZE 32

class BRN2PacketQueueControl : public BRNElement {

 public:

   class Flow {
    public:
     int _start;  //in ms, time til flow start
     int _end;    //in ms, time til flow end

     int _packetsize;

     bool _running;

     int _send_packets;

     int _max_bandwidth; // in Bit/s
     int _interval;

     int _queue_empty;

     uint32_t _id;

     Timestamp _start_ts;
     Timestamp _end_ts;

     Flow(int start, int end, int packetsize, int interval ) {
       _start = start;
       _end = end;
       _packetsize = packetsize;
       _running = false;
       _send_packets = 0;
       _interval = interval;
       _queue_empty = 0;
     }
   };

  BRN2PacketQueueControl();
  ~BRN2PacketQueueControl();

  const char *class_name() const  { return "BRN2PacketQueueControl"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  static void static_queue_timer_hook(Timer *, void *);
  static void static_flow_timer_hook(Timer *, void *);

  int initialize(ErrorHandler *);
  void add_handlers();

  //
  //methods
  //

  void setFlow(Flow *f);
  Flow *get_flow() { return ac_flow; }
  void handle_flow_timer();
  void handle_queue_timer();

  String flow_stats();

  uint32_t _min_count_p;
  uint32_t _max_count_p;

  Timer _queue_timer;
  Timer _flow_timer;

 private:

  HandlerCall* _queue_size_handler;
  HandlerCall* _queue_reset_handler;

  Flow *ac_flow;

  Packet *create_packet(int size);

  uint32_t _flow_id;

 public:
  int _packetheadersize;
};

CLICK_ENDDECLS
#endif
