#ifndef CLICK_BRNRADIOTAPDECAP_HH
#define CLICK_BRNRADIOTAPDECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
BrnRadiotapDecap()

=s Wifi

Pulls the click_wifi_radiotap header from a packet and stores it in Packet::anno()

=d
Removes the radiotap header and copies to to Packet->anno(). This contains
informatino such as rssi, noise, bitrate, etc.

=a RadiotapEncap
*/

class BrnRadiotapDecap : public Element { public:

  BrnRadiotapDecap();
  ~BrnRadiotapDecap();

  const char *class_name() const	{ return "BrnRadiotapDecap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();


  bool _debug;
 private:

};

CLICK_ENDDECLS
#endif
