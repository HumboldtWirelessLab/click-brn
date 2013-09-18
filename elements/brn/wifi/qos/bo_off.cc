#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>

#if CLICK_NS
#include <click/router.hh>
#endif

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"

#include "backoff_scheme.hh"
#include "bo_off.hh"


CLICK_DECLS


BoOff::BoOff()
{
  BRNElement::init();
}


BoOff::~BoOff()
{
}


void * BoOff::cast(const char *name)
{
  if (strcmp(name, "BoOff") == 0)
    return (BoOff *) this;
  else if (strcmp(name, "BackoffScheme") == 0)
         return (BackoffScheme *) this;
       else
         return NULL;
}


int BoOff::configure(Vector<String> &conf, ErrorHandler* errh)
{

  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}


void BoOff::add_handlers()
{
}


uint16_t BoOff::get_id()
{
  return _id;
}


int BoOff::get_cwmin(Packet *p, uint8_t tos)
{
  BRN_DEBUG("BoOff.get_cwmin(): -\n");

  switch (tos) {
    case 0: return 1;
    case 1: return 0;
    case 2: return 2;
    case 3: return 3;
    default: return 1;
  }
}


void BoOff::handle_feedback(uint8_t retries)
{
}



CLICK_ENDDECLS

EXPORT_ELEMENT(BoOff)
ELEMENT_MT_SAFE(BoOff)
