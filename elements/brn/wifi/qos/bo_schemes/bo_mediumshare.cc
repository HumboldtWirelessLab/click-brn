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
    _hnd(NULL),
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
      "HIDDENNODE", cpkP+cpkM, cpElement, &_hnd,
      "BO", cpkP+cpkM, cpInteger, &_current_bo,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}

void BoMediumShare::add_handlers()
{
  BRNElement::add_handlers();
}

bool BoMediumShare::stats_are_new(struct airtime_stats *as)
{
  if (as->stats_id < 2)
    return false;

  if (as->stats_id == _last_id_cw)
    return false;

  _last_id_cw = as->stats_id;
  _last_id_hf = as->stats_id;

  return true;
}

void BoMediumShare::print_reg_info(struct airtime_stats *as)
{
  int own_tx = as->hw_tx;
  int own_rx = as->hw_rx;

  BRN_DEBUG("  current bo: %d\n", _current_bo);
  BRN_DEBUG("  own tx: %d own rx: %d\n", own_tx, own_rx);
}

void BoMediumShare::limit_bo(int lower, int upper)
{
  if (_current_bo < lower)
    _current_bo = lower;

  if (_current_bo > upper)
    _current_bo = upper;

  return;
}

void BoMediumShare::cohesion_decision(int tx_sum, int own_tx, int mean_tx)
{
  if (tx_sum > 100) {
    BRN_DEBUG("  >> own decision: increase (sum > 100)");
    _bo_decision++;
    BRN_DEBUG("  current bo_decision: %d\n", _bo_decision);
    return;
  }

  if (own_tx < mean_tx) {
    BRN_DEBUG("  >> own decision: decrease");
    _bo_decision--;
    BRN_DEBUG("  current bo_decision: %d\n", _bo_decision);
  }

  if (own_tx > mean_tx) {
    BRN_DEBUG("  >> own decision: increase");
    _bo_decision++;
    BRN_DEBUG("  current bo_decision: %d\n", _bo_decision);
  }

  if (own_tx == mean_tx) {
    BRN_DEBUG("  >> own decision: no change");
    BRN_DEBUG("     current bo_decision: %d\n", _bo_decision);
  }

  return;
}

void BoMediumShare::eval_all_rules(void)
{
  if (_bo_decision < 0) {
    BRN_DEBUG("  decrease\n");
    decrease_cw();
    return;
  }

  if (_bo_decision == 0) {
    BRN_DEBUG("  no change. current bo: %d\n", _current_bo);
    return;
  }

  if (_bo_decision > 0) {
    BRN_DEBUG("  increase\n");
    increase_cw();
    return;
  }

  return;
}

void BoMediumShare::print_2hop_tx_dur(NodeChannelStats *ncst)
{
    NeighbourStatsMap *nsm = ncst->get_last_neighbour_map();

    /* for every 2hop nb get tx duration in percent */
    for( NeighbourStatsMapIter iter_m = nsm->begin(); iter_m.live(); iter_m++) {
      EtherAddress n_ea = iter_m.key();
      struct neighbour_airtime_stats *n_nas = iter_m.value();
      double duration_percent = (double)n_nas->_duration / 1000000;
      // / duration_sum;

      BRN_DEBUG("OLI: 2hop duration_percent for %s is now : %f\n", n_ea.unparse().c_str(), duration_percent);
    }
}

EtherAddress BoMediumShare::get_src_etheraddr(Packet *p)
{
    struct click_wifi *w = (struct click_wifi *) p->data();
    return EtherAddress(w->i_addr2);
}

EtherAddress BoMediumShare::get_dst_etheraddr(Packet *p)
{
    struct click_wifi *w = (struct click_wifi *) p->data();
    return EtherAddress(w->i_addr1);
}

int BoMediumShare::get_cwmin(Packet *p, uint8_t tos)
{
  (void) p;
  (void) tos;

  struct airtime_stats *as = _cst->get_latest_stats();

  if (!stats_are_new(as))
    return _current_bo;

  BRN_DEBUG("BoMediumShare::get_cwmin():\n");
  print_reg_info(as);

  EtherAddress my_ea = get_src_etheraddr(p);
  EtherAddress dst_ea = get_dst_etheraddr(p);

  int hns_detected = _hnd->count_hidden_neighbours(dst_ea);

  /* for every nb: sum tx register and if HN also sum tx dur of 2hops */
  NodeChannelStatsMap *nb_cst_map = _cocst->get_stats_map();
  NodeChannelStatsMapIter nb_cst_map_iter = nb_cst_map->begin();
  int own_tx = as->hw_tx;
  int tx_sum = own_tx;
  int num_contenders = 1;

  for (; nb_cst_map_iter != nb_cst_map->end(); ++nb_cst_map_iter) {
    const char* nb = nb_cst_map_iter.key().unparse().c_str();
    NodeChannelStats *node_cst = nb_cst_map_iter.value();
    struct local_airtime_stats *nb_las = node_cst->get_last_stats();

    if (strcmp(nb, "00-00-00-00-00-01")) {  /* ignore receiver */
      BRN_DEBUG("  Nb: %s tx: %d  \trx: %d\n", nb, nb_las->hw_tx, nb_las->hw_rx);

      tx_sum += nb_las->hw_tx;
      num_contenders++;
    }

    if (hns_detected) {
      NeighbourStatsMap *nb_stats_map = node_cst->get_last_neighbour_map();
      NeighbourStatsMapIter nb_stats_map_iter = nb_stats_map->begin();

      for (; nb_stats_map_iter != nb_stats_map->end(); ++nb_stats_map_iter) {
          EtherAddress nb_ea = nb_stats_map_iter.key();

          if (nb_ea == my_ea)
            continue;

          if (nb_ea == brn_etheraddress_broadcast)
            continue;

          struct neighbour_airtime_stats *nb_as = nb_stats_map_iter.value();

          double dur_percent = nb_as->_duration / 1000000.0;
          dur_percent *= 100;
          dur_percent = (int) dur_percent;

          BRN_DEBUG("    2hop Nb: %s tx: %d\n", nb_ea.unparse().c_str(), (int) dur_percent);

          tx_sum += dur_percent;
          num_contenders++;
      }
    }
  }

  int mean_tx = tx_sum / num_contenders;

  BRN_DEBUG("  mean tx: %d  (tx sum: %d no contenders: %d)\n", mean_tx, tx_sum, num_contenders);

  cohesion_decision(tx_sum, own_tx, mean_tx);
  eval_all_rules();
  limit_bo(32, 8096);

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

void BoMediumShare::kohaesion(uint32_t mean_tx, uint32_t own_tx)
{
  _kohaesion_value = 1 - (mean_tx - own_tx) / 100;
  if (_kohaesion_value < 0)
    _kohaesion_value *= -1;
}

void BoMediumShare::gravitation(uint32_t own_tx)
{
  _gravity_value = 1 - (100 - own_tx) / 100;
}

void BoMediumShare::seperation(uint32_t own_tx, uint32_t retries)
{
  // SCD!!
  _seperation_value = ((_retry_sum * own_tx) - (retries * own_tx)) / (_retry_sum * own_tx);
  // HN!! TODO!!!
  //_seperation_value = (( _retry_sum * own_tx) - (retries * own_tx)) / (_retry_sum * own_tx);
}

void BoMediumShare::calc_new_bo()
{
  float y = ( _alpha * _kohaesion_value + _beta * _gravity_value + _gamma * _seperation_value ) / 3;
  if (y < 0.5)
    increase_cw();
  else
    decrease_cw();
}

CLICK_ENDDECLS

EXPORT_ELEMENT(BoMediumShare)
ELEMENT_MT_SAFE(BoMediumShare)
