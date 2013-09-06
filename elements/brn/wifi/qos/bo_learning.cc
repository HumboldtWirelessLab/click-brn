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
#include "bo_learning.hh"

CLICK_DECLS


BoLearning::BoLearning(struct bo_scheme_utils scheme_utils) :
  BackoffScheme(scheme_utils),
  _current_bo(BOLEARNING_STARTING_BO)
{
  //click_chatter("BoL: Constructor! _debug = %d\n", _debug);
  BRNElement::init();
}


BoLearning::BoLearning() :
  BackoffScheme()
{
  BRNElement::init();
}


BoLearning::~BoLearning()
{
}


int BoLearning::get_cwmin()
{
  return _current_bo;
}


void BoLearning::handle_feedback(Packet *p)
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

  uint32_t min_cwmin = _cwmin[0];           //1
  uint32_t max_cwmin = _cwmin[_no_queues-1];
  uint32_t retry_limit = 1;
  uint8_t retries = ceh->retries;

  click_chatter("\nBoL.handle_feedback():");
  click_chatter("\tretries: %d\n", retries);
  click_chatter("\tcurrent bo: %d\n", _current_bo);

  // no retries: reduces bo
  if (retries < retry_limit && _current_bo > min_cwmin) {
    _bo_cnt_down++;
    _current_bo = _current_bo >> 1;

  // 1 retry: don't change cwmin yet
  } else if (retries == retry_limit) {
    //_bo_cnt_down++;
    //_current_bo = _learning_current_bo >> 1;
    //_current_bo = _current_bo;       //keep

  // many retries but still not cwmax: double cwmin
  } else if (retries > retry_limit && _current_bo < max_cwmin) {
    if (BOLEARNING_STRICT) {
      _bo_cnt_up += (retries - retry_limit);
      _current_bo = _current_bo << (retries - retry_limit);
    } else {
      _bo_cnt_up++;
      _current_bo = _current_bo << 1;
    }
  }

  if (_current_bo < BOLEARNING_MIN_CWMIN)
    _current_bo = BOLEARNING_MIN_CWMIN;
  else if (_current_bo > BOLEARNING_MAX_CWMIN)
    _current_bo = BOLEARNING_MAX_CWMIN;

  click_chatter("\tnew bo: %d\n\n", _current_bo);
}


struct bos_learning_stats BoLearning::get_stats()
{
  struct bos_learning_stats learning_stats;
  learning_stats.feedback_cnt = _feedback_cnt;
  learning_stats.tx_cnt       = _tx_cnt;
  learning_stats.bo_cnt_up    = _bo_cnt_up;
  learning_stats.bo_cnt_down  = _bo_cnt_down;

  return learning_stats;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(BoLearning)
ELEMENT_MT_SAFE(BoLearning)
