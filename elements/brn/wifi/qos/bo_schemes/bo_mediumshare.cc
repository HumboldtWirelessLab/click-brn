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
    _last_id(-1)
{
  BRNElement::init();
  _default_strategy = BACKOFF_STRATEGY_MEDIUMSHARE;

  memset(&_ms_info, 0, sizeof(struct ms_info_s));
}

BoMediumShare::~BoMediumShare()
{}

void * BoMediumShare::cast(const char *name)
{
  if (strcmp(name, "BoMediumShare") == 0)
    return (BoMediumShare *) this;

  return BackoffScheme::cast(name);
}

static int
tx_handler(void *element, const EtherAddress *ea, char *buffer, int size);

static int
rx_handler(void *element, EtherAddress *ea, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/);


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

  if (as->stats_id == _last_id)
    return _current_bo;

  _last_id = as->stats_id;

  BRN_DEBUG("BoMediumShare::get_cwmin():\n");

  int own_tx = as->hw_tx;

  BRN_DEBUG("  current bo: %d\n", _current_bo);
  BRN_DEBUG("  own tx: %d\n", own_tx);

  NodeChannelStatsMap *ncstm = _cocst->get_stats_map();
  NodeChannelStatsMapIter ncstm_iter = ncstm->begin();

  int tx_sum = own_tx;
  int no_nbs = 1;

  for (; ncstm_iter != ncstm->end(); ++ncstm_iter) {

    const char* nb = ncstm_iter.key().unparse().c_str();
    NodeChannelStats *ncst = ncstm_iter.value();

    struct local_airtime_stats *nb_cst = ncst->get_last_stats();

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

    if (nb_cst->hw_tx == 0)
      return _current_bo;


    BRN_DEBUG("  Nb: %s tx: %d\n", nb, nb_cst->hw_tx);

    tx_sum += nb_cst->hw_tx;
    no_nbs++;
  }

  int mean_tx = tx_sum / no_nbs;

  BRN_DEBUG("  tx sum: %d no nbs: %d mean tx: %d\n", tx_sum, no_nbs, mean_tx);

  if (own_tx < mean_tx) {
    BRN_DEBUG("  decrease");
    decrease_cw();
  } else if (own_tx > mean_tx) {
    BRN_DEBUG("  increase");
    increase_cw();
  }

  if (_current_bo < 32)
    _current_bo = 32;

  BRN_DEBUG("new bo: %d\n", _current_bo);
  if (_debug >= BrnLogger::DEBUG)
    click_chatter("\n");

  return _current_bo;
}

void BoMediumShare::handle_feedback(uint8_t retries)
{
  if (retries > 0) {
    BRN_DEBUG("BoMediumShare::handle_feedback():");
    BRN_DEBUG("  retries: %d current bo: %d - increase - new bo: %d\n", retries, _current_bo, _current_bo * 2);
    if (_debug >= BrnLogger::DEBUG)
      click_chatter("\n");

    increase_cw();
  }
}

void BoMediumShare::set_conf(uint32_t min, uint32_t max)
{
  _min_cwmin = min;
  _max_cwmin = max;
}

int BoMediumShare::lpSendHandler(char *buffer, int size, const EtherAddress *ea)
{
  (void) ea;

  BRN_DEBUG("BoMediumShare::lpSendHandler()\n");

  struct ms_info_s *ms_info = (struct ms_info_s *) buffer;

  if (size < (int)sizeof(struct ms_info_s)) {
    BRN_WARN("BoMediumShare.lpSendHandler():\n");
    BRN_WARN("No Space for ms_info struct  in Linkprobe\n");
    return 0;
  }

  BRN_DEBUG("  sending medium share info\n");

  ms_info->last_tx = _last_tx;

  return sizeof(struct ms_info_s);
}

int BoMediumShare::lpReceiveHandler(char *buffer, int size, EtherAddress *ea)
{
  (void) size;

  BRN_DEBUG("BoMediumShare::lpReceiveHandler()");

  struct ms_info_s *ms_info = (struct ms_info_s *) buffer;

  BRN_DEBUG("  received tx: %d from %s\n", ms_info->last_tx, ea->unparse().c_str());



  return sizeof(struct ms_info_s);
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


static int
tx_handler(void *element, const EtherAddress *ea, char *buffer, int size)
{
  BoMediumShare *bo_ms = (BoMediumShare *) element;
  return bo_ms->lpSendHandler(buffer, size, ea);
}

static int
rx_handler(void *element, EtherAddress *ea, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  BoMediumShare *bo_ms = (BoMediumShare *) element;
  return bo_ms->lpReceiveHandler(buffer, size, ea);
}


CLICK_ENDDECLS

EXPORT_ELEMENT(BoMediumShare)
ELEMENT_MT_SAFE(BoMediumShare)
