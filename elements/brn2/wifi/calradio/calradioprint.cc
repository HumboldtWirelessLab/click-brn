#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn2/wifi/brnwifi.hh>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "calradioprint.hh"
#include "ieee80211_monitor_calradio.h"

CLICK_DECLS

CalradioPrint::CalradioPrint()
  :_debug(BrnLogger::DEFAULT)
{
}

CalradioPrint::~CalradioPrint()
{
}

int
CalradioPrint::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
  return ret;
}

Packet *
CalradioPrint::simple_action(Packet *p)
{
  struct calradio_header *crh = (struct calradio_header *)p->data();

  click_chatter("RSSI/Power: %d Rate: %d", crh->rssi, crh->rate);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CalradioPrint)
ELEMENT_MT_SAFE(CalradioPrint)
