#ifndef CLICK_BRN2BEACONSOURCE_HH
#define CLICK_BRN2BEACONSOURCE_HH
#include <click/element.hh>
#include <clicknet/ether.h>

#include <elements/brn/vlan/brnvlan/brnvlan.hh>
#include "elements/brn/wifi/brn2_setchannel.hh"

CLICK_DECLS

/*
=c

BRN2BeaconSource([, I<KEYWORDS>])

=s Wifi, Wireless AccessPoint

Send 802.11 beacons.

=d

Keyword arguments are: 

=over 8

=item CHANNEL
The wireless channel it is operating on.

=item SSID
The SSID of the access point.

=item BSSID
An Ethernet Address (usually the same as the ethernet address of the wireless card).

=item INTERVAL
How often beacon packets are sent, in milliseconds.

=back 8

=a BeaconScanner

*/

#define WIFI_ELEMID_CSA  37

class BRN2BeaconSource : public Element { public:
  
  BRN2BeaconSource();
  ~BRN2BeaconSource();

  const char *class_name() const	{ return "BRN2BeaconSource"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers();
  void run_timer(Timer *);
  int initialize (ErrorHandler *);

  void send_beacon(EtherAddress, bool, String);
  void push(int, Packet *);


  Timer _timer;
  bool _debug;
  bool _active;
  uint32_t _target_channel;
  uint32_t _switch_channel_countdown;
  
  EtherAddress _bcast;
  String scan_string();

  class WirelessInfo *_winfo;
  class AvailableRates *_rtable;

  private:

  BRNVLAN* _brn_vlan;

  bool _switch_channel;
  BRN2SetChannel *_sc;


};

CLICK_ENDDECLS
#endif
