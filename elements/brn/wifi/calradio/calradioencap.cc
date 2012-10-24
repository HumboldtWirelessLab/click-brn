#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn/wifi/brnwifi.hh>
#include <elements/brn/brnprotocol/brnpacketanno.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "calradioencap.hh"
#include "ieee80211_monitor_calradio.h"

CLICK_DECLS

CalradioEncap::CalradioEncap()
  :_debug(BrnLogger::DEFAULT)
{
}

CalradioEncap::~CalradioEncap()
{
}

int
CalradioEncap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
  return ret;
}

Packet *
CalradioEncap::simple_action(Packet *p)
{
  WritablePacket *q;
  struct calradio_header *crh = NULL;
  click_wifi_extra *ceh = NULL;

  q = p->push(sizeof(struct calradio_header));

  if ( !q ) {
    p->kill();
    return NULL;
  }

  ceh = WIFI_EXTRA_ANNO(q);
  crh = (struct calradio_header *)q->data();

  crh->rate = ceh->rate;
  crh->rssi = ceh->power;

  return q;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CalradioEncap)
ELEMENT_MT_SAFE(CalradioEncap)
