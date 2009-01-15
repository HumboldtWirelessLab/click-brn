#ifndef CLICK_WMMWIFIENCAP_HH
#define CLICK_WMMWIFIENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

class WMMWifiEncap : public Element { public:

  WMMWifiEncap();
  ~WMMWifiEncap();

  const char *class_name() const	{ return "WMMWifiEncap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();

  uint8_t qos;
  uint8_t queue;

  bool _debug;

  unsigned _mode;
  EtherAddress _bssid;
  class WirelessInfo *_winfo;
 private:

};

CLICK_ENDDECLS
#endif
