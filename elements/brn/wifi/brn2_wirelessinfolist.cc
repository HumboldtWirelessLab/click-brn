#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/vector.hh>
#include "brn2_wirelessinfolist.hh"
#include <elements/wifi/wirelessinfo.hh>

CLICK_DECLS

BRN2WirelessInfoList::BRN2WirelessInfoList()
: _channel(0),_debug(0)
{
}

BRN2WirelessInfoList::~BRN2WirelessInfoList()
{
}

int
BRN2WirelessInfoList::configure(Vector<String> &conf, ErrorHandler *errh)
{
  class WirelessInfo *_winfo = NULL;
  _debug = false;

  if (cp_va_kparse(conf, this, errh,
      "WIRELESS_INFO", cpkP, cpElement, &_winfo,
      "DEBUG", cpkP, cpBool, &_debug,
      cpEnd) < 0)
    return -1;

  if ( _winfo ) {
    _wifiInfoList.push_back(WifiInfo(_winfo->_ssid, _winfo->_bssid, _winfo->_interval, _winfo->_wep, 0 ));
  }

  return 0;
}

Timestamp
BRN2WirelessInfoList::getNextBeaconTime() {
  Timestamp next;
  Timestamp ac;

  if ( _wifiInfoList.size() > 0 ) {
    BRN2WirelessInfoList::WifiInfo wi = _wifiInfoList[0];
    next = wi._send_last + Timestamp::make_msec(wi._interval);
    for ( int i = 1; i < _wifiInfoList.size(); i++ ) {
      wi = _wifiInfoList[i];
      ac = wi._send_last + Timestamp::make_msec(wi._interval);
      if ( ac < next )
        next = ac;
    }
  }

  return next;
}

BRN2WirelessInfoList::WifiInfo*
BRN2WirelessInfoList::getWifiInfoForBSSID(String bssid) {
  for ( int i = 0; i < _wifiInfoList.size(); i++ ) {
    BRN2WirelessInfoList::WifiInfo wi = _wifiInfoList[i];
    if ( wi._ssid == bssid ) return &(_wifiInfoList[i]);
  }

  return NULL;
}

bool
BRN2WirelessInfoList::includesBSSID(String /*bssid*/) {
  return false;
}

BRN2WirelessInfoList::WifiInfo *
BRN2WirelessInfoList::getWifiInfo(int index) {
  if ( index < _wifiInfoList.size() )
    return &(_wifiInfoList[index]);

  return NULL;
}

enum {
  H_DEBUG,
  H_READ,
  H_INSERT,
  H_REMOVE };

static String
BRN2WirelessInfoList_read_param(Element *e, void *thunk)
{
  BRN2WirelessInfoList *wil = reinterpret_cast<BRN2WirelessInfoList *>(e);
  switch ((uintptr_t) thunk) {
    case H_DEBUG: {
      return String(wil->_debug) + "\n";
    }
    case H_READ: {
      StringAccum sa;
      for ( int i = 0; i < wil->_wifiInfoList.size(); i++ ) {
        BRN2WirelessInfoList::WifiInfo wi = wil->_wifiInfoList[i];
        sa << "SSID: " << wi._ssid;
        sa << "\tBSSID: " << wi._bssid.unparse();
        sa << "\tVLAN: " << (uint32_t)wi._vlan;
        sa << "\n";
      }

      return sa.take_string();
   }
   default:
    return String();
  }
}

static int
BRN2WirelessInfoList_write_param(const String &in_s, Element *e, void *vparam,
                                 ErrorHandler *errh)
{
  BRN2WirelessInfoList *f = reinterpret_cast<BRN2WirelessInfoList *>(e);
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {
      bool debug;
      if (!cp_bool(s, &debug))
        return errh->error("debug parameter must be boolean");
      f->_debug = debug;
      break;
    }
    case H_INSERT: {
      Vector<String> args;
      cp_spacevec(s, args);

      String ssid = args[0];
      EtherAddress bssid;
      int interval;
      bool wep;
      uint32_t vlan;

      cp_ethernet_address(args[1], &bssid);
      cp_integer(args[2], &interval);
      cp_bool(args[3], &wep);
      cp_integer(args[4], &vlan);

      f->_wifiInfoList.push_back(BRN2WirelessInfoList::WifiInfo(ssid, bssid, interval, wep, vlan ));
      break;
    }
  }

  return 0;
}

void
BRN2WirelessInfoList::add_handlers()
{
  add_read_handler("debug", BRN2WirelessInfoList_read_param, (void *) H_DEBUG);
  add_read_handler("info", BRN2WirelessInfoList_read_param, (void *) H_READ);

  add_write_handler("debug", BRN2WirelessInfoList_write_param, (void *) H_DEBUG);
  add_write_handler("insert", BRN2WirelessInfoList_write_param, (void *) H_INSERT);
}

#include <click/vector.cc>
template class Vector<BRN2WirelessInfoList::WifiInfo>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2WirelessInfoList)

