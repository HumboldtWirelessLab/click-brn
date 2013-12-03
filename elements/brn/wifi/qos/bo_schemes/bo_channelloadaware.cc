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
#include "bo_channelloadaware.hh"


CLICK_DECLS



BoChannelLoadAware::BoChannelLoadAware()
  : _cst(NULL),
    _strategy(0),
    _current_bo(_bo_start),
    _target_busy(0),
    _target_diff(0),
    _last_diff(0),
    _cap(0),
    _cst_sync(0),
    _last_id(9999)
{
  BRNElement::init();
}

BoChannelLoadAware::~BoChannelLoadAware()
{
}

void * BoChannelLoadAware::cast(const char *name)
{
  if (strcmp(name, "BoChannelLoadAware") == 0)
    return (BoChannelLoadAware *) this;
  else if (strcmp(name, "BackoffScheme") == 0)
         return (BackoffScheme *) this;
       else
         return NULL;
}

int BoChannelLoadAware::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
      "TARGETLOAD", cpkP, cpInteger, &_target_busy,
      "TARGETDIFF", cpkP, cpInteger, &_target_diff,
      "CAP", cpkP, cpInteger, &_cap,
      "CST_SYNC", cpkP, cpInteger, &_cst_sync,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}

void BoChannelLoadAware::add_handlers()
{
}

bool BoChannelLoadAware::handle_strategy(uint32_t strategy)
{
  switch(strategy) {
  case BACKOFF_STRATEGY_BUSY_AWARE:
  case BACKOFF_STRATEGY_TARGET_DIFF_RXTX_BUSY:
  case BACKOFF_STRATEGY_TX_AWARE:
    return true;
  default:
    return false;
  }
}

int BoChannelLoadAware::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  struct airtime_stats *as = _cst->get_latest_stats();

  if (_cst_sync && (as->stats_id == _last_id))
    return _current_bo;

  _last_id = as->stats_id;


  BRN_DEBUG("BoChannelLoadAware.get_cwmin():\n");
  BRN_DEBUG("    old bo: %d\n", _current_bo);

  switch(_strategy) {
  case BACKOFF_STRATEGY_BUSY_AWARE: {
    uint32_t wiggle_room = _target_busy / 10;
    BRN_DEBUG("    busy: %d _target_channel: %d wm param %d\n", as->hw_busy, _target_busy, wiggle_room);
    if ((as->hw_busy < (_target_busy - wiggle_room)) && ((int32_t)_current_bo > 1))
      decrease_cw();
    else if (as->hw_busy > (_target_busy + wiggle_room))
      increase_cw();
    break;

  } case BACKOFF_STRATEGY_TARGET_DIFF_RXTX_BUSY: {
    int32_t diff = as->hw_busy - (as->hw_tx + as->hw_rx);
    if (diff < 0)
      diff = 0;

    uint32_t wiggle_room = 0; // no. nbs?

    //BRN_DEBUG("    rxtxbusy: %d %d %d -> diff: %d _last diff %d _target_diff: %d wm param %d\n", as->hw_rx, as->hw_tx, as->hw_busy, diff, _last_diff, _target_diff, wiggle_room);
    BRN_DEBUG("    rxtxbusy: %d %d %d -> diff: %d _last diff %d\n", as->hw_rx, as->hw_tx, as->hw_busy, diff, _last_diff);

    if ((diff == 0) && (_last_diff == 0))
      decrease_cw();
    else if ((diff == 0) && (_last_diff != 0))
      _current_bo = _current_bo;
    else if (diff < (int)(_last_diff - wiggle_room))
      decrease_cw();
    else if (diff > (int)(_last_diff + wiggle_room))
      increase_cw();

    _last_diff = diff;
    break;
/*
    if ((diff < (int)(_target_diff - wiggle_room)) && ((int32_t)_current_bo > 1))
      decrease_cw();
    else if (diff > (int)(_target_diff + wiggle_room))
      increase_cw();
    break;
*/

  } case BACKOFF_STRATEGY_TX_AWARE: {
    BRN_DEBUG("    tx: %d nbs = %d\n", as->hw_tx, as->no_sources);

    if (as->no_sources == 0)
      break;

    uint32_t target_tx = 100 / as->no_sources;
    uint32_t wiggle_room = target_tx / 10;

    BRN_DEBUG("    tx: %d target tx: %d wm param: %d\n", as->hw_tx, target_tx, wiggle_room);
    if (as->hw_tx < (target_tx - wiggle_room))
      decrease_cw();
    else if (as->hw_tx > (target_tx + wiggle_room))
      increase_cw();
    break;

  } default:
    BRN_DEBUG("    ERROR: no matching strategy found");
  }

  if (_cap) {
    if (_current_bo < _cla_min_cwmin)
      _current_bo = _cla_min_cwmin;
    else if (_current_bo > _cla_max_cwmin)
      _current_bo = _cla_max_cwmin;
  }

  BRN_DEBUG("    new bo: %d\n\n", _current_bo);

  return _current_bo;
}

void BoChannelLoadAware::handle_feedback(uint8_t retries)
{
  (void) retries;
}

void BoChannelLoadAware::set_conf(uint32_t min, uint32_t max)
{
  _min_cwmin = min;
  _max_cwmin = max;
}

void BoChannelLoadAware::set_strategy(uint32_t strategy)
{
  _strategy = strategy;
}

void BoChannelLoadAware::increase_cw()
{
  _current_bo = _current_bo << 1;
}

void BoChannelLoadAware::decrease_cw()
{
  _current_bo = _current_bo >> 1;
}



CLICK_ENDDECLS

EXPORT_ELEMENT(BoChannelLoadAware)
ELEMENT_MT_SAFE(BoChannelLoadAware)
