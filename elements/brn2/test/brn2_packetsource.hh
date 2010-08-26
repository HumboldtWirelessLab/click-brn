#ifndef BRN2PACKETSOURCEELEMENT_HH
#define BRN2PACKETSOURCEELEMENT_HH

#include <click/element.hh>
#include <click/timer.hh>

CLICK_DECLS

/*
 * =c
 * BRN2PacketSource()
 * =s
 * SEND Packet with given size
 * =d
 */

class BRN2PacketSource : public Element {

 public:

   int _debug;
  //
  //methods
  //

/* brn2_packetsource.cc**/

  BRN2PacketSource();
  ~BRN2PacketSource();

  const char *class_name() const  { return "BRN2PacketSource"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "0-1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void run_timer(Timer *t);

  int initialize(ErrorHandler *);
  void add_handlers();
  void push( int port, Packet *packet );

  bool _active;

 private:
  int _interval;

  uint32_t _size;

  uint32_t _seq_num;

  uint32_t _max_seq_num;

  uint32_t _channel,_bitrate,_power;

  Timer _timer;

  Packet *createpacket(int size);

  struct packetinfo {
    uint32_t seq_num;
    uint16_t interval;
    uint8_t channel;
    uint8_t bitrate;
    uint8_t power;
  };

};

CLICK_ENDDECLS
#endif
