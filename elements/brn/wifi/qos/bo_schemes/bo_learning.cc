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
  _strategy(0),
  _strict(0),
  _current_bo(_bo_start),
  _bo_cnt_up(0),
  _bo_cnt_down(0),
  _learning_min_cwmin(31),
  _learning_max_cwmin(511),
  _cap(0)
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
  if (cp_va_kparse(conf, this, errh,
      "MIN_CWMIN", cpkP, cpInteger, &_learning_min_cwmin,
      "MAX_CWMIN", cpkP, cpInteger, &_learning_max_cwmin,
      "STRICT", cpkP, cpInteger, &_strict,
      "CAP", cpkP, cpInteger, &_cap,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;


  return 0;
}


void BoLearning::add_handlers()
{
}


bool BoLearning::handle_strategy(uint32_t strategy)
{
  return (strategy == BACKOFF_STRATEGY_LEARNING) ? true : false;
}


int BoLearning::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  return _current_bo;
}


void BoLearning::handle_feedback(uint8_t retries)
{
  BRN_DEBUG("BoL.handle_feedback():");
  BRN_DEBUG("    retries: %d\n", retries);
  BRN_DEBUG("    current bo: %d\n", _current_bo);

  if (retries < _retry_threshold && _current_bo > 1)
    decrease_cw();
  else if (retries == _retry_threshold)
    //keep_cw();
    increase_cw();
  else if (retries > _retry_threshold) {
    if (_strict)
      increase_cw_strict(retries);
    else
      increase_cw();
  }

  if (_cap) {
    if (_current_bo < _learning_min_cwmin) {
      _current_bo = _learning_min_cwmin;
    } else if (_current_bo > _learning_max_cwmin)
      _current_bo = _learning_max_cwmin;
  }

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
  _current_bo = _current_bo << retries;
}


void BoLearning::decrease_cw()
{
  _bo_cnt_down++;
  _current_bo = _current_bo >> 1;
}

void BoLearning::keep_cw()
{
}

void BoLearning::set_strategy(uint32_t strategy)
{
  _strategy = strategy;
}


CLICK_ENDDECLS

EXPORT_ELEMENT(BoLearning)
ELEMENT_MT_SAFE(BoLearning)
