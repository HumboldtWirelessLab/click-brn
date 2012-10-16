#ifndef CLICK_WIRELESSINFOLIST_HH
#define CLICK_WIRELESSINFOLIST_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include <click/vector.hh>

CLICK_DECLS

//RESPONSIBLE: Robert

/*TODO:
  - handle to remove WifiInfos
  - check whether there is another WifiInfo  using the vlan-id
  - what can be private
*/

class BRN2WirelessInfoList : public Element {

 public:
  class WifiInfo {

   public:

    String _ssid;
    EtherAddress _bssid;
    int _interval;
    bool _wep;
    bool _protected;

    uint16_t _vlan;

    Timestamp _send_last;

    WifiInfo() {
    }

    WifiInfo( String ssid, EtherAddress bssid, int interval, bool wep, uint8_t vlan) {
      _ssid = ssid;
      _bssid = bssid;
      _interval = interval;
      _wep = wep;
      _vlan = vlan;
      _protected = false;
      updateTime();
    }

    void updateTime() { _send_last = Timestamp::now(); }
  };

  typedef Vector<WifiInfo> WifiInfoList;

  BRN2WirelessInfoList();
  ~BRN2WirelessInfoList();

  const char *class_name() const		{ return "BRN2WirelessInfoList"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();

  Timestamp getNextBeaconTime();

  WifiInfo *getWifiInfoForBSSID(String bssid);
  WifiInfo *getWifiInfo(int index);
  bool includesBSSID(String bssid);
  int countWifiInfo() { return _wifiInfoList.size(); }

  WifiInfoList _wifiInfoList;
  int _channel;
  int _debug;
};

CLICK_ENDDECLS
#endif
