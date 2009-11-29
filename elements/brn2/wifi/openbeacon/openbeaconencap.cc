#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "openbeaconencap.hh"
#include "ieee80211_monitor_openbeacon.h"

CLICK_DECLS

OpenBeaconEncap::OpenBeaconEncap()
  :_debug(BrnLogger::DEFAULT)
{
}

OpenBeaconEncap::~OpenBeaconEncap()
{
}

int
OpenBeaconEncap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
  return ret;
}

Packet *
OpenBeaconEncap::simple_action(Packet *p)
{
  WritablePacket *q;
  struct openbeacon_header *crh = NULL;
  click_wifi_extra *ceh = NULL;

  q = p->push(sizeof(struct openbeacon_header));

  if ( !q ) {
    p->kill();
    return NULL;
  }

  ceh = WIFI_EXTRA_ANNO(q);
  crh = (struct openbeacon_header *)q->data();

  crh->rate = ceh->rate;
  crh->rssi = ceh->power;

  return q;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OpenBeaconEncap)
ELEMENT_MT_SAFE(OpenBeaconEncap)
