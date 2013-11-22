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
#include "bo_nbs.hh"


CLICK_DECLS



BoNeighbours::BoNeighbours()
  : _cst(NULL)
{
  BRNElement::init();
}


BoNeighbours::~BoNeighbours()
{
}

void * BoNeighbours::cast(const char *name)
{
  if (strcmp(name, "BoNeighbours") == 0)
    return (BoNeighbours *) this;
  else if (strcmp(name, "BackoffScheme") == 0)
         return (BackoffScheme *) this;
       else
         return NULL;
}


int BoNeighbours::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}


void BoNeighbours::add_handlers()
{
}


bool BoNeighbours::handle_strategy(uint32_t strategy)
{
  return (strategy == BACKOFF_STRATEGY_NEIGHBOURS) ? true : false;
}


int BoNeighbours::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  struct airtime_stats *as = _cst->get_latest_stats();
  int32_t nbs = as->no_sources;

  int curr_cwmin = ((alpha * nbs) - beta) / 10;

  if (curr_cwmin < (int)_min_cwmin)
    curr_cwmin = _min_cwmin;
  else if (curr_cwmin > (int)_max_cwmin)
    curr_cwmin = _max_cwmin;

  BRN_DEBUG("BoNeighbours.get_cwmin():");
  BRN_DEBUG("    nbs: %d\n", nbs);
  BRN_DEBUG("    cwmin: %d\n", curr_cwmin);

  return curr_cwmin;
}


void BoNeighbours::handle_feedback(uint8_t retries)
{
  (void) retries;
}


void BoNeighbours::set_conf(uint32_t min, uint32_t max)
{
  _min_cwmin = min;
  _max_cwmin = max;
}

void BoNeighbours::set_strategy(uint32_t strategy)
{
  _strategy = strategy;
}

CLICK_ENDDECLS

EXPORT_ELEMENT(BoNeighbours)
ELEMENT_MT_SAFE(BoNeighbours)
