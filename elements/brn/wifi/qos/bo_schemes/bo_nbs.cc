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

#include "bo_nbs.hh"

CLICK_DECLS

BoNeighbours::BoNeighbours()
  : _cst(NULL),
    _cst_sync(0),
    _last_id(9999),
    _current_bo(BO_NEIGHBOURS_START_BO),
    _alpha(BO_NEIGHBOURS_ALPHA),
    _beta(BO_NEIGHBOURS_BETA)
{
  BRNElement::init();
  _default_strategy = BACKOFF_STRATEGY_NEIGHBOURS;
}

BoNeighbours::~BoNeighbours()
{
}

void * BoNeighbours::cast(const char *name)
{
  if (strcmp(name, "BoNeighbours") == 0)
    return (BoNeighbours *) this;

  return BackoffScheme::cast(name);
}

int BoNeighbours::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
      "CST_SYNC", cpkP, cpInteger, &_cst_sync,
      "ALPHA", cpkP, cpInteger, &_alpha,
      "BETA", cpkP, cpInteger, &_beta,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}

int BoNeighbours::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  struct airtime_stats *as = _cst->get_latest_stats();
  BRN_DEBUG("stats_id: %d\n", as->stats_id);

  if (_cst_sync && (as->stats_id == _last_id))
    return _current_bo;

  _last_id = as->stats_id;
  int32_t nbs = as->no_sources;

  _current_bo = ((_alpha * nbs) - _beta) / 10;
  BRN_DEBUG("formular bo: %d\n", _current_bo);

  if (_current_bo < (int)_min_cwmin)      _current_bo = _min_cwmin;
  else if (_current_bo > (int)_max_cwmin) _current_bo = _max_cwmin;

  BRN_DEBUG("BoNeighbours.get_cwmin():");
  BRN_DEBUG("    nbs: %d\n", nbs);
  BRN_DEBUG("    cwmin: %d\n", _current_bo);

  return _current_bo;
}

CLICK_ENDDECLS

EXPORT_ELEMENT(BoNeighbours)
ELEMENT_MT_SAFE(BoNeighbours)
