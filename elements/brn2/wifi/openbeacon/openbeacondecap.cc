#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "openbeacondecap.hh"
#include "ieee80211_monitor_openbeacon.h"

CLICK_DECLS

OpenBeaconDecap::OpenBeaconDecap()
  :_debug(BrnLogger::DEFAULT)
{
}

OpenBeaconDecap::~OpenBeaconDecap()
{
}

int
OpenBeaconDecap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
  return ret;
}

Packet *
OpenBeaconDecap::simple_action(Packet *p)
{
  p->pull(sizeof(struct openbeacon_header));

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OpenBeaconDecap)
ELEMENT_MT_SAFE(OpenBeaconDecap)
