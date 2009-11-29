#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

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
  p->pull(sizeof(struct calradio_header));

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CalradioDecap)
ELEMENT_MT_SAFE(CalradioDecap)
