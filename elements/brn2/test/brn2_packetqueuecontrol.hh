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
     int _start;
     int _end;
     int _time_now;

     int _packetsize;

     bool _running;

     int _send_packets;

     Flow(int start, int end, int packetsize ) {
       _start = start;
       _end = end;
       _time_now = start;
       _packetsize = packetsize;
       _running = false;
       _send_packets = 0;
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

  static void static_flow_queue_timer_hook(Timer *, void *);

  int initialize(ErrorHandler *);
  void add_handlers();

  void setFlow(Flow *f);
  Flow *getAcFlow() { return acflow; }
  void handle_flow_timer();
  void handle_queue_timer();

  int _interval;

 private:

  Timer _flow_queue_timer;
  HandlerCall* _queue_size_handler;
  HandlerCall* _queue_reset_handler;

  Flow *acflow;

  uint32_t _min_count_p;
  uint32_t _max_count_p;

  Packet *createpacket(int size);

};

CLICK_ENDDECLS
#endif
