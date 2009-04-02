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

  void run_timer(Timer *t);

  int initialize(ErrorHandler *);
  void add_handlers();

 private:

  Timer _timer;
  HandlerCall*   _queue_size_handler;

  int _interval;

  uint32_t _packet_size;

  uint32_t _min_count_p;
  uint32_t _max_count_p;

  Packet *createpacket(int size);

};

CLICK_ENDDECLS
#endif
