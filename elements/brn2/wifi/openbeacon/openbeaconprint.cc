#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "openbeaconprint.hh"
#include "ieee80211_monitor_openbeacon.h"

CLICK_DECLS

OpenBeaconPrint::OpenBeaconPrint()
  :_debug(BrnLogger::DEFAULT)
{
}

OpenBeaconPrint::~OpenBeaconPrint()
{
}

int
OpenBeaconPrint::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
  return ret;
}

Packet *
OpenBeaconPrint::simple_action(Packet *p)
{
  struct openbeacon_header *crh = (struct openbeacon_header *)p->data();

  click_chatter("RSSI/Power: %d Rate: %d", crh->rssi, crh->rate);

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OpenBeaconPrint)
ELEMENT_MT_SAFE(OpenBeaconPrint)
