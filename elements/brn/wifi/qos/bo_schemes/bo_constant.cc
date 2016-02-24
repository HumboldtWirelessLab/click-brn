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

#include "bo_constant.hh"

CLICK_DECLS

BoConstant::BoConstant()
  : _const_bo(0)
{
  BRNElement::init();
  _default_strategy = BACKOFF_STRATEGY_CONSTANT;
}


BoConstant::~BoConstant()
{
}

void * BoConstant::cast(const char *name)
{
  if (strcmp(name, "BoConstant") == 0)
    return dynamic_cast<BoConstant *>(this);

  return BackoffScheme::cast(name);
}

int BoConstant::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "BO", cpkP+cpkM, cpUnsignedShort, &_const_bo,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}

void BoConstant::add_handlers()
{
}

int BoConstant::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  return _const_bo;
}

CLICK_ENDDECLS

EXPORT_ELEMENT(BoConstant)
ELEMENT_MT_SAFE(BoConstant)
