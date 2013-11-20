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
    _target_channelload(0),
    _bo_for_target_channelload(_bo_start),
    _tcl_lwm(0),
    _tcl_hwm(0)
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
      "TARGETLOAD", cpkP+cpkM, cpInteger, &_target_channelload,
      "CAP", cpkP, cpInteger, &_cap,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  /* target channel load low & high water marks needed to calc a new cwmin */
  _tcl_lwm = (int32_t) _target_channelload - _target_load_param;
  _tcl_hwm = (int32_t) _target_channelload + _target_load_param;

  return 0;
}


void BoChannelLoadAware::add_handlers()
{
}


uint16_t BoChannelLoadAware::get_id()
{
  return _id;
}


int BoChannelLoadAware::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  struct airtime_stats *as = _cst->get_latest_stats();
  uint32_t busy = as->hw_busy;

  BRN_DEBUG("BoChannelLoadAware.get_cwmin():\n");
  BRN_DEBUG("    busy: %d _target_channel: %d\n", busy, _target_channelload);
  BRN_DEBUG("    old bo: %d\n", _bo_for_target_channelload);

  /*
   * if we're under the targeted channel load, decrease the contention window
   * to increase the channel load. If we're over the specified target,
   * increase the contention window to lower the channel load.
   */
  if ((busy < _tcl_lwm) && ((int32_t)_bo_for_target_channelload > 1))
    decrease_cw();
  else if (busy > _tcl_hwm)
    increase_cw();

  if (_cap) {
    if (_bo_for_target_channelload < _tcl_min_cwmin)
      _bo_for_target_channelload = _tcl_min_cwmin;
    else if (_bo_for_target_channelload > _tcl_max_cwmin)
      _bo_for_target_channelload = _tcl_max_cwmin;
  }

  BRN_DEBUG("    new bo: %d\n\n", _bo_for_target_channelload);

  return _bo_for_target_channelload;
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


void BoChannelLoadAware::increase_cw()
{
  _bo_for_target_channelload = _bo_for_target_channelload << 1;
}


void BoChannelLoadAware::decrease_cw()
{
  _bo_for_target_channelload = _bo_for_target_channelload >> 1;
}



CLICK_ENDDECLS

EXPORT_ELEMENT(BoChannelLoadAware)
ELEMENT_MT_SAFE(BoChannelLoadAware)
