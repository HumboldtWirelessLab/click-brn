#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn/wifi/brnwifi.hh>
#include <elements/brn/brnprotocol/brnpacketanno.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "calradiodecap.hh"
#include "ieee80211_monitor_calradio.h"

CLICK_DECLS

CalradioDecap::CalradioDecap()
  :_debug(BrnLogger::DEFAULT)
{
}

CalradioDecap::~CalradioDecap()
{
}

int
CalradioDecap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
  return ret;
}

Packet *
CalradioDecap::simple_action(Packet *p)
{
  struct calradio_header *crh;
  click_wifi_extra *eh;

  crh = (struct calradio_header*)p->data();

  eh = WIFI_EXTRA_ANNO(p);
  crh->rssi = eh->rssi;
  crh->rate = eh->rate;

  p->pull(sizeof(struct calradio_header));

  return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel linux)
EXPORT_ELEMENT(CalradioDecap)
ELEMENT_MT_SAFE(CalradioDecap)
