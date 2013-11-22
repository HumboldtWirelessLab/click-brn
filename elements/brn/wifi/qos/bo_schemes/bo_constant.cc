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
#include "elements/brn/wifi/channelstats.hh"
#include "elements/brn/brn2.h"

#include "backoff_scheme.hh"
#include "bo_constant.hh"


CLICK_DECLS



BoConstant::BoConstant()
  : _strategy(0),
    _const_bo(0)
{
  BRNElement::init();
}


BoConstant::~BoConstant()
{
}

void * BoConstant::cast(const char *name)
{
  if (strcmp(name, "BoConstant") == 0)
    return (BoConstant *) this;
  else if (strcmp(name, "BackoffScheme") == 0)
         return (BackoffScheme *) this;
       else
         return NULL;
}


int BoConstant::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
      "BO", cpkP+cpkM, cpInteger, &_const_bo,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}


void BoConstant::add_handlers()
{
}


bool BoConstant::handle_strategy(uint32_t strategy)
{
  return (strategy == BACKOFF_STRATEGY_CONSTANT) ? true : false;
}


int BoConstant::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  struct airtime_stats *as = _cst->get_latest_stats();
  uint32_t busy = as->hw_busy;

  BRN_DEBUG("BoConst.get_cwmin():\n");
  BRN_DEBUG("      busy: %d\n", busy);
  BRN_DEBUG("    old bo: %d\n", _const_bo);
  BRN_DEBUG("    new bo: %d\n\n", _const_bo);

  return _const_bo;
}


void BoConstant::handle_feedback(uint8_t retries)
{
  (void) retries;
}


void BoConstant::set_conf(uint32_t min, uint32_t max)
{
  _min_cwmin = min;
  _max_cwmin = max;
}

void BoConstant::set_strategy(uint32_t strategy)
{
  _strategy = strategy;
}


CLICK_ENDDECLS

EXPORT_ELEMENT(BoConstant)
ELEMENT_MT_SAFE(BoConstant)
