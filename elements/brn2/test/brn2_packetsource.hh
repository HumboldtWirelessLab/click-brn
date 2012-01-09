#ifndef BRN2PACKETSOURCEELEMENT_HH
#define BRN2PACKETSOURCEELEMENT_HH

#include <click/element.hh>
#include <click/timer.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

/*
 * =c
 * BRN2PacketSource()
 * =s
 * SEND Packet with given size
 * =d
 */

class BRN2PacketSource : public BRNElement {

 public:

/* brn2_packetsource.cc**/

  BRN2PacketSource();
  ~BRN2PacketSource();

  const char *class_name() const  { return "BRN2PacketSource"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "0-1/1-2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void run_timer(Timer *t);

  int initialize(ErrorHandler *);
  void add_handlers();
  void push( int port, Packet *packet );

  bool _active;

  void set_active(bool set_active);

 private:
  int _interval;

  uint32_t _size;

  uint32_t _seq_num;

  uint32_t _max_seq_num;

  Timer _timer;

  uint32_t _burst,_channel,_bitrate,_power;

  uint32_t _headroom;

  uint32_t _max_packets;
  uint32_t _send_packets;

  Packet *createpacket(int size);

  struct packetinfo {
    uint16_t interval;

    uint32_t seq_num;

    uint8_t channel;
    uint8_t bitrate;
    uint8_t power;
    uint8_t reserved[3];
  } __attribute__ ((packed));

  struct packetinfo pinfo;

  bool _reuse;
  uint32_t _reuseoffset;

};

CLICK_ENDDECLS
#endif
