#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"

#include "bo_flooding.hh"
#include "tos2qm_data.hh"

CLICK_DECLS

BoFlooding::BoFlooding()
//:
//  _flooding(NULL),
//  _fhelper(NULL)
{
  BRNElement::init();
  _default_strategy = BACKOFF_STRATEGY_FLOODING;
}

BoFlooding::~BoFlooding()
{
}

void * BoFlooding::cast(const char *name)
{
  if (strcmp(name, "BoFlooding") == 0)
    return dynamic_cast<BoFlooding *>(this);

  return BackoffScheme::cast(name);
}

int BoFlooding::configure(Vector<String> &conf, ErrorHandler* errh)
{

  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}

int BoFlooding::get_cwmin(Packet */*p*/, uint8_t tos)
{
  (void) tos;

  return 32;
}

CLICK_ENDDECLS

EXPORT_ELEMENT(BoFlooding)
ELEMENT_MT_SAFE(BoFlooding)
