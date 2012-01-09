#ifndef CLICK_BRNRADIOTAPENCAP_HH
#define CLICK_BRNRADIOTAPENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
BrnRadiotapEncap()

=s Wifi

Pushes the click_wifi_radiotap header on a packet based on information in Packet::anno()

=d

Copies the wifi_radiotap_header from Packet::anno() and pushes it onto the packet.

=a RadiotapDecap, SetTXRate
*/


class BrnRadiotapEncap : public Element { public:

  BrnRadiotapEncap();
  ~BrnRadiotapEncap();

  const char *class_name() const	{ return "BrnRadiotapEncap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();

  int _mcs_known;
  bool _debug;

};

CLICK_ENDDECLS
#endif
