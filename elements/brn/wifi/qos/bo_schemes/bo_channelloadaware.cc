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

#include "bo_channelloadaware.hh"


CLICK_DECLS


BoChannelLoadAware::BoChannelLoadAware()
  : _cst(NULL),
    _current_bo(BO_CHANNELLOADAWARE_START_BO),
    _target_busy(0),
    _target_diff(0),
    _last_diff(0),
    _cap(0),
    _cst_sync(0),
    _last_id(999)
{
  BRNElement::init();
  _default_strategy = BACKOFF_STRATEGY_TX_AWARE;
}

BoChannelLoadAware::~BoChannelLoadAware()
{
}

void * BoChannelLoadAware::cast(const char *name)
{
  if (strcmp(name, "BoChannelLoadAware") == 0)
    return (BoChannelLoadAware *) this;

  return BackoffScheme::cast(name);
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

/*
static uint32_t find_closest_backoff(uint32_t bo)
{
  uint32_t c_bo = 1;

  while (c_bo < bo)
    c_bo = c_bo << 1;

  return c_bo;
}
*/

int BoChannelLoadAware::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  struct airtime_stats *as = _cst->get_latest_stats();

  if (_cst_sync && (as->stats_id == _last_id))
    return _current_bo - 1;

  _last_id = as->stats_id;

  BRN_DEBUG("BoChannelLoadAware.get_cwmin():\n");
  BRN_DEBUG("    old bo: %d\n", _current_bo);

  switch(_strategy) {
  case BACKOFF_STRATEGY_BUSY_AWARE: {
    uint32_t wiggle_room = _target_busy / 20; // 5%

    BRN_DEBUG("    busy: %d _target_channel: %d wm param %d\n", as->hw_busy, _target_busy, wiggle_room);

    if ((as->hw_busy < (_target_busy - wiggle_room)) && ((int32_t)_current_bo > 1))
    //if ((as->hw_busy < _target_busy) && ((int32_t)_current_bo > 1))
      decrease_cw();
    //else if (as->hw_busy > (_target_busy + wiggle_room))
    else if (as->hw_busy > _target_busy)
      increase_cw();
    break;

  } case BACKOFF_STRATEGY_TARGET_DIFF_RXTX_BUSY: {
    int32_t diff = as->hw_busy - (as->hw_tx + as->hw_rx);

    if (diff < 0) diff = 0;

    uint32_t wiggle_room = 5; // no. nbs?
    uint32_t target_diff_low = (_target_diff - wiggle_room);
    uint32_t target_diff_up = (_target_diff + wiggle_room);

    BRN_DEBUG("    rxtxbusy: %d %d %d -> diff: %d _target diff %d\n", as->hw_rx, as->hw_tx, as->hw_busy, diff, _target_diff);

    if ((uint32_t)diff < target_diff_low) decrease_cw();
    else if ((uint32_t)diff > target_diff_up) increase_cw();

    break;

  } case BACKOFF_STRATEGY_TX_AWARE: {
    BRN_DEBUG("    tx: %d nbs = %d\n", as->frac_mac_tx, as->no_sources);

    if (as->no_sources == 0) break;

    uint32_t hw_tx = 1000 * as->frac_mac_tx;
    uint32_t target_tx = 100000 / (as->no_sources + 1);
    uint32_t wiggle_room = target_tx / 10;


    BRN_DEBUG("    tx: %d target tx: %d wm param: %d\n", hw_tx, target_tx, wiggle_room);

    if (hw_tx < (target_tx - wiggle_room)) decrease_cw();
    else if (hw_tx > target_tx)            increase_cw();

    break;

  } case BACKOFF_STRATEGY_BUSY_TX_AWARE: {
    uint32_t wiggle_room = _target_busy / 20; // 5%

    BRN_DEBUG("    busy: %d _target_channel: %d wm param %d\n", as->hw_busy, _target_busy, wiggle_room);

    if ((as->hw_busy < (_target_busy - wiggle_room)) && ((int32_t)_current_bo > 1))
    //if ((as->hw_busy < _target_busy) && ((int32_t)_current_bo > 1))
      decrease_cw();
    //else if (as->hw_busy > (_target_busy + wiggle_room))
    else if (as->hw_busy > _target_busy)
      increase_cw();
    break;

  } default:
    BRN_DEBUG("    ERROR: no matching strategy found");
  }

  if (_cap) {
    //uint16_t lower_bound = find_closest_backoff(2 * as->no_sources);

    if ((uint32_t)_current_bo < _min_cwmin)
      _current_bo = _min_cwmin;
  }

  BRN_DEBUG("    new bo: %d\n\n", _current_bo);

  return _current_bo - 1;
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
