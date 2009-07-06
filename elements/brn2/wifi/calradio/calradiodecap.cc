#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>

#include "calradiodecap.hh"
#include "ieee80211_monitor_calradio.h"

CLICK_DECLS

CalradioDecap::CalradioDecap()
{
}

CalradioDecap::~CalradioDecap()
{
}

int
CalradioDecap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  bool _debug = false;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUF", cpkP, cpBool, &_debug,
                     cpEnd);
  return ret;
}

Packet *
CalradioDecap::simple_action(Packet *p)
{
  WritablePacket *q;
  click_wifi_extra *eh;
  struct calradio_header *crh = NULL;


  q = p->uniqueify();
  if ( !q ) return p;

  crh = (struct calradio_header *)q->data();
  eh = WIFI_EXTRA_ANNO(q);
  memset(eh, 0, sizeof(click_wifi_extra));
  eh->magic = WIFI_EXTRA_MAGIC;

  eh->rate = crh->rate;
  eh->rssi = crh->rate;

  q->pull(sizeof(struct calradio_header));

  return q;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CalradioDecap)
ELEMENT_MT_SAFE(CalradioDecap)
