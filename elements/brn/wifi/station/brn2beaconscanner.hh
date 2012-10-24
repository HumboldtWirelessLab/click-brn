#ifndef CLICK_BRN2BEACONSCANNER_HH
#define CLICK_BRN2BEACONSCANNER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
#include <elements/wifi/wirelessinfo.hh>
#include "elements/brn/wifi/brnavailablerates.hh"

CLICK_DECLS

/*
=c

BRN2BeaconScanner

=s Wifi, Wireless Station

Listens for 802.11 beacons and sends probe requests.

=d

=h scan read only
Statistics about access points that the element has received beacons from.

=h reset read/write
Clear the list of access points.

=h channel
If the channel is greater than 0, it will only record statistics for 
beacons received with that channel in the packet.
If channel is 0, it will record statistics for all beacons received.
If channel is less than 0, it will discard all beaconds


=e

  FromDevice(ath0) 
  -> Prism2Decap()
  -> ExtraDecap()
  -> Classifier(0/80%f0)  // only beacon packets
  -> bs :: BRN2BeaconScanner()
  -> Discard;

=a

EtherEncap */

/** Beaconscanner with Virtual AP - Support ( multiSSID for one BSSID )
  * TODO: clean up ( remove old list for ap and just use pap/vap-struct
  * respo: robert
*/

class BRN2BeaconScanner : public Element { public:

  BRN2BeaconScanner();
  ~BRN2BeaconScanner();

  const char *class_name() const	{ return "BRN2BeaconScanner"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();
  void reset();

  int _debug;

  String scan_string();
  String scan_string2();

 private:
 public:

  /**Virtual are identify by the ssid*/
  class vap {
  public:
    EtherAddress _eth;
    String _ssid;
    bool _ssid_empty;
    int _channel;
    uint16_t _capability;
    uint16_t _beacon_int;
    Vector<MCS> _rates;
    Vector<MCS> _basic_rates;
    int _rssi;
    Timestamp _last_rx;

    Vector<int> _recv_channel;
    Vector<int> _rssi_list;  //last rssi-values for statistics
  };

  typedef HashMap<String, vap> VAPTable;
  typedef VAPTable::const_iterator VAPIter;

  /**physical APs are identify dy the bssid (MAC)*/
  class pap {
    public:
      EtherAddress _eth;
      VAPTable _vaps;

      Timestamp _last_rx;
  };

  typedef HashMap<EtherAddress, pap> PAPTable;
  typedef PAPTable::const_iterator PAPIter;

  typedef HashMap<EtherAddress, vap> APTable;
  typedef APTable::const_iterator APIter;

  APTable _aps;
  PAPTable _paps;

  BrnAvailableRates *_rtable;
  WirelessInfo *_winfo;

};

CLICK_ENDDECLS
#endif
