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
#include "bo_targetdiff_rxtxbusy.hh"

CLICK_DECLS




BoTargetDiffRxTxBusy::BoTargetDiffRxTxBusy()
  : _cst(NULL),
    _targetdiff(0),
    _bo_for_targetdiff(_bo_start),
    _tdiff_lwm(0),
    _tdiff_hwm(0)
{
  BRNElement::init();
}


BoTargetDiffRxTxBusy::~BoTargetDiffRxTxBusy()
{
}


void * BoTargetDiffRxTxBusy::cast(const char *name)
{
  if (strcmp(name, "BoTargetDiffRxTxBusy") == 0)
    return (BoTargetDiffRxTxBusy *) this;
  else if (strcmp(name, "BackoffScheme") == 0)
         return (BackoffScheme *) this;
       else
         return NULL;
}


int BoTargetDiffRxTxBusy::configure(Vector<String> &conf, ErrorHandler* errh)
{

  if (cp_va_kparse(conf, this, errh,
      "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
      "TARGETDIFF", cpkP+cpkM, cpInteger, &_targetdiff,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  /* target diff low & high water marks needed to calc a new cwmin */
  _tdiff_lwm = (int32_t) _targetdiff - _targetdiff_param;
  _tdiff_hwm = (int32_t) _targetdiff + _targetdiff_param;

  return 0;
}


void BoTargetDiffRxTxBusy::add_handlers()
{
}

uint16_t BoTargetDiffRxTxBusy::get_id()
{
  return _id;
}


int BoTargetDiffRxTxBusy::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  struct airtime_stats *as = _cst->get_latest_stats();
  uint32_t busy = as->hw_busy;
  uint32_t rx   = as->hw_rx;
  uint32_t tx   = as->hw_tx;

  uint32_t diff = busy - (tx + rx);

  BRN_DEBUG("BoTargetDiff.get_cwmin():\n");
  BRN_DEBUG("    rxtxbusy: %d %d %d -> diff: %d _target_diff: %d\n", rx, tx, busy, diff, _targetdiff);
  BRN_DEBUG("    old bo: %d\n", _bo_for_targetdiff);

  if ((diff < _tdiff_lwm) && ((int32_t)_bo_for_targetdiff > 1))
    decrease_cw();
  else if ((diff > _tdiff_hwm) && (_bo_for_targetdiff < _max_cwmin))
    increase_cw();

  if (_bo_for_targetdiff < _min_cwmin)
    _bo_for_targetdiff = _min_cwmin;
  else if (_bo_for_targetdiff > _max_cwmin)
    _bo_for_targetdiff = _max_cwmin;

  BRN_DEBUG("    new bo: %d\n\n", _bo_for_targetdiff);

  return _bo_for_targetdiff;
}


void BoTargetDiffRxTxBusy::handle_feedback(uint8_t retries)
{
  (void) retries;
}


void BoTargetDiffRxTxBusy::set_conf(uint32_t min, uint32_t max)
{
  _min_cwmin = min;
  _max_cwmin = max;
}


void BoTargetDiffRxTxBusy::increase_cw()
{
  _bo_for_targetdiff = _bo_for_targetdiff << 1;
}


void BoTargetDiffRxTxBusy::decrease_cw()
{
  _bo_for_targetdiff = _bo_for_targetdiff >> 1;
}


CLICK_ENDDECLS

EXPORT_ELEMENT(BoTargetDiffRxTxBusy)
ELEMENT_MT_SAFE(BoTargetDiffRxTxBusy)
