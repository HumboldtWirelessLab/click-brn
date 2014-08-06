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

#include "bo_minstrel.hh"

CLICK_DECLS

BoMinstrel::BoMinstrel()
{
  BRNElement::init();
  _default_strategy = BACKOFF_STRATEGY_MINSTREL;

  _best_bo = 31;
  _best_bo_exp = 5;
  _stats_index = 0;
  _last_update = Timestamp::now();
}

BoMinstrel::~BoMinstrel()
{
}

void * BoMinstrel::cast(const char *name)
{
  if (strcmp(name, "BoMinstrel") == 0)
    return (BoMinstrel *) this;

  return BackoffScheme::cast(name);
}

int BoMinstrel::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret = Args(conf, this, errh)
      .read("DEBUG", _debug)
      .complete();
  return ret;

  click_brn_srandom();
}

void BoMinstrel::add_handlers()
{
}


int BoMinstrel::get_cwmin(Packet */*p*/, uint8_t /*tos*/)
{
  Timestamp now = Timestamp::now();

  if ((now-_last_update).msecval() > BO_MINSTREL_DEFAULT_STATSINTERVAL) {
    _bostats[_stats_index].calc_stats();

    int best_bo_succ_rate = 0;
    int best_bo_index = -1;

    for(int i = 0; i < BOSTATS_MAX_BO_EXP; i++) {
      if (_bostats[_stats_index].succ_rate[i] > best_bo_succ_rate) {
        best_bo_succ_rate = _bostats[_stats_index].succ_rate[i];
        best_bo_index = i;
      }
    }

    if ( best_bo_index != -1 ) {
      _best_bo_exp = best_bo_index;
      _best_bo = (1 << best_bo_index) - 1;
    }

    _stats_index = (_stats_index+1)%BO_MINSTREL_DEFAULT_STATSSIZE;
    _last_update = now;
  }

  int bo = _best_bo;
  int bo_exp = _best_bo_exp;

  if ((click_random() % 100) < 10) {
    int sample_bo = (click_random() % 2);
    if ((sample_bo == 0) && (bo_exp > 4))      bo_exp = bo_exp - 1 - sample_bo;
    else if ((sample_bo == 1) && (bo_exp < 14)) bo_exp = bo_exp - 1 + sample_bo;

    bo = (1 << bo_exp) - 1;
  }

  _last_bo = bo;
  _last_bo_exp = bo_exp;

  BRN_DEBUG("BO: %d BOEXP: %d",_last_bo,_last_bo_exp);

  return bo;
}

void BoMinstrel::handle_feedback(uint8_t retries)
{
  BRN_DEBUG("Last BO: %d Last BOEXP: %d retries: %d",_last_bo,_last_bo_exp,retries);

  _bostats[_stats_index].add_stats(_last_bo_exp, _last_bo_exp + retries, true);
}

CLICK_ENDDECLS

EXPORT_ELEMENT(BoMinstrel)
ELEMENT_MT_SAFE(BoMinstrel)
