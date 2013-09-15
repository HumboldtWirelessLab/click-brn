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
#include "bo_learning.hh"


CLICK_DECLS



BoLearning::BoLearning() :
  _current_bo(_bo_start),
  _bo_cnt_up(0),
  _bo_cnt_down(0),
  _strict(0)
{
  BRNElement::init();
}


BoLearning::~BoLearning()
{
}


void * BoLearning::cast(const char *name)
{
  if (strcmp(name, "BoLearning") == 0)
    return (BoLearning *) this;
  else if (strcmp(name, "BackoffScheme") == 0)
         return (BackoffScheme *) this;
       else
         return NULL;
}


int BoLearning::configure(Vector<String> &conf, ErrorHandler* errh)
{
  uint32_t min_cwmin = -1;
  uint32_t max_cwmin = -1;

  if (cp_va_kparse(conf, this, errh,
      "MIN_CWMIN", cpkP, cpInteger, &min_cwmin,
      "MAX_CWMIN", cpkP, cpInteger, &max_cwmin,
      "STRICT", cpkP, cpInteger, &_strict,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  if ((min_cwmin > 0) && (max_cwmin) > 0)
    set_conf(min_cwmin, max_cwmin);

  return 0;
}


void BoLearning::add_handlers()
{
}


uint16_t BoLearning::get_id()
{
  return _id;
}


int BoLearning::get_cwmin(Packet *p, uint8_t tos)
{
  return _current_bo;
}


void BoLearning::handle_feedback(uint8_t retries)
{
  BRN_DEBUG("BoL.handle_feedback():");
  BRN_DEBUG("    retries: %d\n", retries);
  BRN_DEBUG("    current bo: %d\n", _current_bo);

  if (retries < _retry_threshold && _current_bo > _min_cwmin)
    decrease_cw();
  else if (retries == _retry_threshold)
    keep_cw();
  else if (retries > _retry_threshold && _current_bo < _max_cwmin) {
    if (_strict)
      increase_cw_strict(retries);
    else
      increase_cw();
  }

  if (_current_bo < _min_cwmin)
    _current_bo = _min_cwmin;
  else if (_current_bo > _max_cwmin)
    _current_bo = _max_cwmin;

  BRN_DEBUG("    new bo: %d\n\n", _current_bo);
}


void BoLearning::set_conf(uint32_t min, uint32_t max)
{
  _min_cwmin = min;
  _max_cwmin = max;
}


void BoLearning::increase_cw()
{
  _bo_cnt_up++;
  _current_bo = _current_bo << 1;
}


void BoLearning::increase_cw_strict(uint8_t retries)
{
  _bo_cnt_up += (retries - _retry_threshold);
  _current_bo = _current_bo << (retries - _retry_threshold);
}


void BoLearning::decrease_cw()
{
  _bo_cnt_down++;
  _current_bo = _current_bo >> 1;
}

void BoLearning::keep_cw()
{
}


CLICK_ENDDECLS

EXPORT_ELEMENT(BoLearning)
ELEMENT_MT_SAFE(BoLearning)
