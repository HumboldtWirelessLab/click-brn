#ifndef BRN2HOTSPOTSCONNECTORELEMENT_HH
#define BRN2HOTSPOTSCONNECTORELEMENT_HH

#include <click/element.hh>
#include <click/timer.hh>

CLICK_DECLS

/*
 * =c
 * BRN2HotSpotsConnector()
 * =s
 * SEND Packet with given size
 * =d
 */

class BRN2HotSpotsConnector : public Element {

 public:

   int _debug;
  //
  //methods
  //

/* brn2_packetsource.cc**/

  BRN2HotSpotsConnector();
  ~BRN2HotSpotsConnector();

  const char *class_name() const  { return "BRN2HotSpotsConnector"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "0/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void run_timer(Timer *t);

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  int _start_offset;
  int _renew_interval;
  IPAddress _click_ip;
  uint16_t _click_port;
  IPAddress _packet_ip;
  uint16_t _packet_port;

  int _state;

  Timer _timer;

  Packet *createpacket();

  struct hscinfo {
    int ipaddr[4];
    uint16_t port;
    int packetipaddr[4];
    uint16_t packetport;
  };

};

CLICK_ENDDECLS
#endif
