#ifndef BRN2PACKETQUEUECONTROLELEMENT_HH
#define BRN2PACKETQUEUECONTROLELEMENT_HH

#include <click/element.hh>
#include <click/timer.hh>
#include <click/handlercall.hh>

CLICK_DECLS

/*
 * =c
 * BRN2PacketQueueControl()
 * =s
 * SEND Packet with given size
 * =d
 */

class BRN2PacketQueueControl : public Element {

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

     Flow(int start, int end, int packetsize, int interval ) {
       _start = start;
       _end = end;
       _packetsize = packetsize;
       _running = false;
       _send_packets = 0;
       _interval = interval;
     }
   };

   int _debug;
  //
  //methods
  //

/* brn2_packetsource.cc**/

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

  void setFlow(Flow *f);
  Flow *getAcFlow() { return acflow; }
  void handle_flow_timer();
  void handle_queue_timer();

  uint32_t _min_count_p;
  uint32_t _max_count_p;

 private:

  Timer _queue_timer;
  Timer _flow_timer;
  HandlerCall* _queue_size_handler;
  HandlerCall* _queue_reset_handler;

  Flow *acflow;

  Packet *createpacket(int size);

};

CLICK_ENDDECLS
#endif
