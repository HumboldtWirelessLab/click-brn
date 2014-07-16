#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/wifi/rxinfo/channelstats/nodechannelstats.hh"

#include "bo_mediumshare.hh"


CLICK_DECLS



BoMediumShare::BoMediumShare()
  : _cocst(NULL),
    _cocst_string(""),
    _current_bo(0),
    _last_tx(0),
    _last_id_cw(-1),
    _last_id_hf(-1),
    _retry_sum(0)
{
  BRNElement::init();
  _default_strategy = BACKOFF_STRATEGY_MEDIUMSHARE;
}

BoMediumShare::~BoMediumShare()
{}

void * BoMediumShare::cast(const char *name)
{
  if (strcmp(name, "BoMediumShare") == 0)
    return (BoMediumShare *) this;

  return BackoffScheme::cast(name);
}

int BoMediumShare::initialize(ErrorHandler *errh)
{
  //_linkstat->registerHandler(this,BRN2_LINKSTAT_MINOR_TYPE_GPS,&tx_handler,&rx_handler);

  if ((_cocst == NULL) && (_cocst_string != "")) {
    Element *e = cp_element(_cocst_string, this, errh);
    _cocst = (CooperativeChannelStats*)e;
  }

  return 0;
}

int BoMediumShare::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
      "COOPCHANNELSTATSPATH", cpkP+cpkM, cpString, &_cocst_string,
      "BO", cpkP+cpkM, cpInteger, &_current_bo,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}

void BoMediumShare::add_handlers()
{
  BRNElement::add_handlers();
}

static bool is_new(Timestamp stats, Timestamp now) {
  Timestamp diff = now - stats;

  return diff.msecval() > 1000;
}

int BoMediumShare::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  struct airtime_stats *as = _cst->get_latest_stats();

  if (as->stats_id < 2)
    return _current_bo;

  if (as->stats_id == _last_id_cw)
    return _current_bo;

  _last_id_cw = as->stats_id;
  _last_id_hf = as->stats_id;

  BRN_DEBUG("BoMediumShare::get_cwmin():\n");

  int own_tx = as->hw_tx;
  int own_rx = as->hw_rx;

  BRN_DEBUG("  current bo: %d\n", _current_bo);
  BRN_DEBUG("  own tx: %d own rx: %d\n", own_tx, own_rx);

  NodeChannelStatsMap *ncstm = _cocst->get_stats_map();
  NodeChannelStatsMapIter ncstm_iter = ncstm->begin();

  int tx_sum = own_tx;
  int no_nbs = 1;

  /* for every neighbour: get register content */
  for (; ncstm_iter != ncstm->end(); ++ncstm_iter) {

    const char* nb = ncstm_iter.key().unparse().c_str();

    NeighbourStatsMap *nsm = ncst->get_last_neighbour_map();

    uint32_t duration_sum = 0;
    for( NeighbourStatsMapIter iter_m = nsm->begin(); iter_m.live(); iter_m++) {
      struct neighbour_airtime_stats *n_nas = iter_m.value();
      duration_sum += (uint32_t)n_nas->_duration;
    }

    for( NeighbourStatsMapIter iter_m = nsm->begin(); iter_m.live(); iter_m++) {
      EtherAddress n_ea = iter_m.key();
      struct neighbour_airtime_stats *n_nas = iter_m.value();
      double duration_percent = (double)n_nas->_duration / 1000000;
      // / duration_sum;

      BRN_DEBUG("OLI: 2hop duration_percent for %s is now : %f\n", n_ea.unparse().c_str(), duration_percent);
    }
/*
    // for SCD only (exclude the receiver cuz he's got 0 tx all the time
    if (!strcmp(nb, "00-00-00-00-00-01"))
      continue;
*/

    if (!strcmp(nb, "00-00-00-00-00-02"))
      continue;

    NodeChannelStats *ncst = ncstm_iter.value();

    struct local_airtime_stats *nb_cst = ncst->get_last_stats();
    BRN_DEBUG("  Nb: %s tx: %d  \trx: %d\n", nb, nb_cst->hw_tx, nb_cst->hw_rx);

    tx_sum += nb_cst->hw_tx;
    no_nbs++;


/* HIDDEN NODE: for every neighbours neighbour calc tx % based on
   tx duration in usec */

    struct click_wifi *w = (struct click_wifi *) p->data();
    EtherAddress my_ea = EtherAddress(w->i_addr2);

    NeighbourStatsMap *nsm = ncst->get_last_neighbour_map();
    NeighbourStatsMapIter nsm_iter = nsm->begin();

    for (; nsm_iter != nsm->end(); ++nsm_iter) {
        EtherAddress n_ea = nsm_iter.key();
        struct neighbour_airtime_stats *nas = nsm_iter.value();

        double dur_percent = nas->_duration / 1000000.0;
        dur_percent *= 100;
        dur_percent = (int) dur_percent;

        if (n_ea == my_ea)
          continue;

        if (n_ea == brn_etheraddress_broadcast)
          continue;

        BRN_DEBUG("    2hop Nb: %s tx: %d\n", n_ea.unparse().c_str(), (int) dur_percent);

        tx_sum += dur_percent;
        no_nbs++;
    }

  }

  int mean_tx = tx_sum / no_nbs;

  BRN_DEBUG("  own_tx: %d mean tx: %d  (tx sum: %d no nbs: %d)\n", own_tx, mean_tx, tx_sum, no_nbs);


  if (tx_sum > 100) {
    BRN_DEBUG("  >> own decision: increase (sum > 100)");
    _bo_decision++;
    BRN_DEBUG("  current bo_decision: %d\n", _bo_decision);
  } else if (own_tx < mean_tx) {
    BRN_DEBUG("  >> own decision: decrease");
    _bo_decision--;
    BRN_DEBUG("  current bo_decision: %d\n", _bo_decision);
  } else  if (own_tx > mean_tx) {
    BRN_DEBUG("  >> own decision: increase");
    _bo_decision++;
    BRN_DEBUG("  current bo_decision: %d\n", _bo_decision);
  } else if (own_tx == mean_tx) {
    BRN_DEBUG("  >> own decision: no change");
    BRN_DEBUG("     current bo_decision: %d\n", _bo_decision);
  }

  if (_bo_decision < 0) {
    BRN_DEBUG("  decrease\n");
    decrease_cw();
  } else if (_bo_decision == 0) {
    BRN_DEBUG("  no change. current bo: %d\n", _current_bo);
  } else if (_bo_decision > 0) {
    BRN_DEBUG("  increase\n");
    increase_cw();
  }

  if (_current_bo < 32)
    _current_bo = 32;

  if (_current_bo > 8096)
    _current_bo = 8096;

  BRN_DEBUG("  new bo: %d\n", _current_bo);

  if (_debug >= BrnLogger::DEBUG)
    click_chatter("\n");

  return _current_bo;
}

void BoMediumShare::handle_feedback(uint8_t retries)
{
  struct airtime_stats *as = _cst->get_latest_stats();

  if (as->stats_id < 2)
    return;

  _retry_sum += retries;

  if (as->stats_id == _last_id_hf)
    return;

  _last_id_hf = as->stats_id;

  _bo_decision = -1; /* Gravitation */

  BRN_DEBUG("BoMediumShare::handle_feedback():");
  BRN_DEBUG("  retry sum: %d\n", _retry_sum);

  if (_retry_sum > 20) {
    BRN_DEBUG("  >> own decision: increase");
    _bo_decision++;
  } else {
    BRN_DEBUG("  >> own decision: no change");
  }

  BRN_DEBUG("  current bo_decision: %d\n", _bo_decision);

  _retry_sum = 0;

  if (_debug >= BrnLogger::DEBUG)
    click_chatter("\n");

  return;
}

void BoMediumShare::set_conf(uint32_t min, uint32_t max)
{
  _min_cwmin = min;
  _max_cwmin = max;
}

void BoMediumShare::increase_cw()
{
  _current_bo = _current_bo << 1;
}

void BoMediumShare::increase_cw_strict(uint8_t retries)
{
  _current_bo = _current_bo << retries;
}

void BoMediumShare::decrease_cw()
{
  _current_bo = _current_bo >> 1;
}


CLICK_ENDDECLS

EXPORT_ELEMENT(BoMediumShare)
ELEMENT_MT_SAFE(BoMediumShare)
