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
  _bo_cnt_down(0)
{
  BRNElement::init();
}


BoLearning::~BoLearning()
{
}


int BoLearning::configure(Vector<String> &conf, ErrorHandler* errh)
{

  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}


void BoLearning::add_handlers()
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


int BoLearning::get_cwmin()
{
  return _current_bo;
}


void BoLearning::handle_feedback(uint8_t retries)
{
  BRN_DEBUG("\nBoL.handle_feedback():");
  BRN_DEBUG("\tretries: %d\n", retries);
  BRN_DEBUG("\tcurrent bo: %d\n", _current_bo);

  if (retries < _retry_threshold && _current_bo > _min_cwmin)
    decrease_cw();
  else if (retries == _retry_threshold)
    keep_cw();
  else if (retries > _retry_threshold && _current_bo < _max_cwmin)
    increase_cw();

  if (_current_bo < _min_cwmin)
    _current_bo = _min_cwmin;
  else if (_current_bo > _max_cwmin)
    _current_bo = _max_cwmin;

  BRN_DEBUG("\tnew bo: %d\n\n", _current_bo);
}



void BoLearning::increase_cw()
{
  _bo_cnt_up++;
  _current_bo = _current_bo << 1;
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
