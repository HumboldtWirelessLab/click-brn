#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"

#include "backoff_scheme.hh"
#include "bo_learning_strict.hh"


CLICK_DECLS



BoLearningStrict::BoLearningStrict(struct bo_scheme_utils scheme_utils) :
  BackoffScheme(scheme_utils),
  _current_bo(_bo_start),
  _pkt_in_q(0),
  _feedback_cnt(0),
  _tx_cnt(0),
  _bo_cnt_up(0),
  _bo_cnt_down(0)
{
  BRNElement::init();
}


BoLearningStrict::BoLearningStrict() :
  BackoffScheme(),
  _current_bo(_bo_start),
  _pkt_in_q(0),
  _feedback_cnt(0),
  _tx_cnt(0),
  _bo_cnt_up(0),
  _bo_cnt_down(0)
{
  BRNElement::init();
}


BoLearningStrict::~BoLearningStrict()
{
}


int BoLearningStrict::get_cwmin()
{
  return _current_bo;
}


void BoLearningStrict::handle_feedback(Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  if (!(ceh->flags & WIFI_EXTRA_TX))
    return;

  struct click_wifi *wh = (struct click_wifi *) p->data();
  if (EtherAddress(wh->i_addr1) == brn_etheraddress_broadcast)
    return;

  _pkt_in_q--;
  _feedback_cnt++;
  _tx_cnt += ceh->retries + 1;

  uint32_t min_cwmin = _cwmin[0];
  uint32_t max_cwmin = _cwmin[_no_queues-1];

  uint8_t retries = ceh->retries;

  click_chatter("\nBoL.handle_feedback():");
  click_chatter("\tretries: %d\n", retries);
  click_chatter("\tcurrent bo: %d\n", _current_bo);

  if (retries < _retry_threshold && _current_bo > min_cwmin)
    decrease_cw();
  else if (retries == _retry_threshold)
    keep_cw();
  else if (retries > _retry_threshold && _current_bo < max_cwmin)
    increase_cw(retries);

  if (_current_bo < _min_cwmin)
    _current_bo = _min_cwmin;
  else if (_current_bo > _max_cwmin)
    _current_bo = _max_cwmin;

  click_chatter("\tnew bo: %d\n\n", _current_bo);
}


struct bos_learningstrict_stats BoLearningStrict::get_stats()
{
  struct bos_learningstrict_stats learning_stats;
  learning_stats.feedback_cnt = _feedback_cnt;
  learning_stats.tx_cnt       = _tx_cnt;
  learning_stats.bo_cnt_up    = _bo_cnt_up;
  learning_stats.bo_cnt_down  = _bo_cnt_down;

  return learning_stats;
}


void BoLearningStrict::increase_cw(uint8_t retries)
{
  _bo_cnt_up += (retries - _retry_threshold);
  _current_bo = _current_bo << (retries - _retry_threshold);
}


void BoLearningStrict::decrease_cw()
{
  _bo_cnt_down++;
  _current_bo = _current_bo >> 1;
}


void BoLearningStrict::keep_cw()
{
}


CLICK_ENDDECLS

EXPORT_ELEMENT(BoLearningStrict)
ELEMENT_MT_SAFE(BoLearningStrict)
