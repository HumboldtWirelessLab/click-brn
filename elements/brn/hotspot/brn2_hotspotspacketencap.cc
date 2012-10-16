#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_hotspotspacketencap.hh"

CLICK_DECLS

BRN2HotSpotsPacketEncap::BRN2HotSpotsPacketEncap()
{
}

BRN2HotSpotsPacketEncap::~BRN2HotSpotsPacketEncap()
{

}

int
BRN2HotSpotsPacketEncap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP+cpkM, cpInteger, &_debug,
      cpEnd) < 0)
        return -1;
 
  return 0;
}

int
BRN2HotSpotsPacketEncap::initialize(ErrorHandler *)
{
  return 0;
}

static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
BRN2HotSpotsPacketEncap::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2HotSpotsPacketEncap)
