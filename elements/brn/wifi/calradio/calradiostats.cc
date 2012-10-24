#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn/wifi/brnwifi.hh>
#include <elements/brn/brnprotocol/brnpacketanno.hh>

#include "calradiostats.hh"
#include "ieee80211_monitor_calradio.h"

CLICK_DECLS

CalradioStats::CalradioStats()
{
}

CalradioStats::~CalradioStats()
{
}

int
CalradioStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  bool _debug = false;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpBool, &_debug,
                     cpEnd);
  return ret;
}

Packet *
CalradioStats::simple_action(Packet *p)
{
    //struct calradio_header *crh = NULL;
    //crh = (struct calradio_header *)p->data();

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CalradioStats)
ELEMENT_MT_SAFE(CalradioStats)
