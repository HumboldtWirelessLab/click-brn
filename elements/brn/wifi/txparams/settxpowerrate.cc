#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>

#include "elements/brn/brn2.h"
#include "elements/brn/standard/fixpointnumber.hh"

#include "settxpowerrate.hh"

CLICK_DECLS

SetTXPowerRate::SetTXPowerRate():
  _rtable(NULL),
  _rate_selection(NULL),
  _rate_selection_strategy(RATESELECTION_NONE),
  _max_power(0),
  _timer(this),
  _stats_timer(this),
  _stats_timer_interval(SETTXPOWERRATE_DEFAULT_STATS_TIMER_INTERVAL),
  _offset(0),
  _has_wifi_header(0)
{
  BRNElement::init();
  _scheme_list = SchemeList(String("RateSelection"));
}

SetTXPowerRate::~SetTXPowerRate()
{
}

int
SetTXPowerRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String rate_selection_string;

  if (cp_va_kparse(conf, this, errh,
      "RATESELECTIONS", cpkM+cpkP, cpString, &rate_selection_string,
      "STRATEGY", cpkP, cpInteger, &_rate_selection_strategy,
      "RT", cpkP, cpElement, &_rtable,
      "POWER", cpkP, cpInteger, &_max_power,
      "OFFSET", cpkP, cpInteger, &_offset,
      "STATSINTERVAL", cpkP, cpInteger, &_stats_timer_interval,
      "DEBUG", 0, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  _scheme_list.set_scheme_string(rate_selection_string);
  _has_wifi_header = ( _offset >= 0 );

  return 0;
}


int
SetTXPowerRate::initialize(ErrorHandler *errh)
{
  _timer.initialize(this);
  _stats_timer.initialize(this);

  _scheme_list.parse_schemes(this, errh);

  //TODO: use extra function for next steps
  _rate_selection = get_rateselection(_rate_selection_strategy);

  if ( _rate_selection ) {
    if ( _rate_selection->get_adjust_period() > 0 ) {
      _timer.schedule_now();
    }
  }

  if (_stats_timer_interval > 0 ) _stats_timer.schedule_after_msec(_stats_timer_interval);

  if ( _rtable != NULL ) _rate_selection->init(_rtable);

  if ( (_max_power == 0) || (_max_power > _rtable->get_max_txpower())) {
    _max_power = _rtable->get_max_txpower();
  }

  return 0;
}

void
SetTXPowerRate::run_timer(Timer *t)
{
  if ( t == &_timer ) {
    _timer.schedule_after_msec(_rate_selection->get_adjust_period());
    _rate_selection->adjust_all(&_neighbors);
  } else if ( t == &_stats_timer) {

    _stats_timer.schedule_after_msec(_stats_timer_interval);

    for (NIter iter = _neighbors.begin(); iter.live(); iter++) {
      NeighbourRateInfo *nri = iter.value();
      nri->last_stats = Timestamp::now();
      nri->stats.set_next_timeslot();
    }

    if ( _rate_selection->get_adjust_period() == RATESELECTION_ADJUST_PERIOD_ON_STATS_UPDATE ) {
      if (_debug >= BrnLogger::DEBUG) click_chatter("%s",getInfo().c_str());
      _rate_selection->adjust_all(&_neighbors);
    }
  } else {
    BRN_ERROR("Unknown Timer");
  }
}

Packet *
SetTXPowerRate::handle_packet(int port, Packet *p)
{
  if ( p == NULL ) return NULL;

  EtherAddress dst;
  EtherAddress src;

  if ( _has_wifi_header ) {
    dst = EtherAddress(((struct click_wifi *) p->data())->i_addr1);
    src = EtherAddress(((struct click_wifi *) p->data())->i_addr2);
  } else {
    dst = EtherAddress(((click_ether*)p->data())->ether_dhost);
    src = EtherAddress(((click_ether*)p->data())->ether_shost);
  }

  struct rateselection_packet_info rs_pkt_info;

  rs_pkt_info.wee = BrnWifi::get_brn_click_wifi_extra_extention(p);
  rs_pkt_info.ceh = WIFI_EXTRA_ANNO(p);
  rs_pkt_info.p = p;
  rs_pkt_info.has_wifi_header = _has_wifi_header;

  NeighbourRateInfo *dsti;

  switch (port) {
    case 0:                                       //Got packet from upper layer
        {
          dsti = getDstInfo(dst);
          _rate_selection->assign_rate(&rs_pkt_info, dsti);
          break;
        }
   case 1:                                        // TXFEEDBACK
        {
          if (!dst.is_group()) {
            dsti = getDstInfo(dst);               //dst of packet is other node (txfeedback)

            update_stats(&rs_pkt_info, dsti);

            _rate_selection->process_feedback(&rs_pkt_info, dsti);
          }
          break;
        }
  case 2:                                         //received for other nodes
        {
          dsti = getDstInfo(src);  //src of packet is other node
          _rate_selection->process_foreign(&rs_pkt_info, dsti);
          break;
        }
  }

  return p;
}

void
SetTXPowerRate::push(int port, Packet *p)
{
  output(port).push(handle_packet(port,p));
}

Packet *
SetTXPowerRate::pull(int port)
{
  return handle_packet(port,input(port).pull());
}

void
SetTXPowerRate::update_stats(struct rateselection_packet_info *rs_pi, NeighbourRateInfo *dsti)
{
  click_wifi_extra * ceh = rs_pi->ceh;

  int tx_count = (int) ceh->retries + 1;
  bool use_rtscts = ((ceh->flags & WIFI_EXTRA_DO_RTS_CTS) != 0);
  MCS mcs;

  uint8_t *triesfield = &ceh->max_tries;

  BRN_DEBUG("Feedback: %d (%d/%d %d/%d %d/%d %d/%d)",(int) ceh->retries, ceh->rate, ceh->max_tries, ceh->rate1, ceh->max_tries1,
                                                                         ceh->rate2, ceh->max_tries2, ceh->rate3, ceh->max_tries3);

  for( int i = 0; i < 4; i++) {
    if (triesfield[i] != 0) {
      mcs.getWifiRate(ceh, i);
      dsti->stats.update_rate(mcs, MIN(triesfield[i], tx_count), (tx_count <= triesfield[i]), use_rtscts);
    }

    if ( (tx_count -= triesfield[i]) <= 0) return;
  }
}

NeighbourRateInfo *
SetTXPowerRate::getDstInfo(EtherAddress ea)
{
  NeighbourRateInfo **dip = _neighbors.findp(ea);

  if ( dip == NULL ) {
    _neighbors.insert(ea, new NeighbourRateInfo(ea, _rtable->lookup(ea), (uint8_t)_max_power));
    dip = _neighbors.findp(ea);
  }

  return *dip;
}

String
SetTXPowerRate::getInfo()
{
  StringAccum sa;
  String rs;

  if ( _rate_selection == NULL )
    rs = "n/a";
  else
    rs = _rate_selection->name();

  sa << "<ratecontrol rateselection=\"" << rs << "\" >\n";
  sa << "\t<neighbours count=\"" << _neighbors.size() << "\" >\n";

  for (NIter iter = _neighbors.begin(); iter.live(); iter++) {
    NeighbourRateInfo *nri = iter.value();
    sa << "\t\t<neighbour node=\"" << nri->_eth.unparse() << "\" >\n\t\t\t<stats ts=\"" << nri->last_stats.unparse() << "\" >\n";

    for (RateStatsMapIter it = nri->stats._ratestatsmap.begin(); it.live(); ++it) {
      MCS mcs = it.key();
      RateStats *rs = it.value();
      RateStats *current_rs = &(rs[nri->stats.last_timeslot]);

      FPN s = FPN(current_rs->psr)/FPN(100000);
      sa << "\t\t\t\t<rate datarate=\"" << mcs._data_rate << "\" psr=\"" << s.unparse();
      sa << "\" tx=\"" << current_rs->count_tx << "\" tx_succss=\"" << current_rs->count_tx_succ;
      sa << "\" eff_tp=\"" << current_rs->throughput << "\" />\n";
    }

    sa << "\t\t\t</stats>\n";

    sa << _rate_selection->print_neighbour_info(nri, 3);

    sa << "\t\t</neighbour>\n";

  }

  sa << "\t</neighbours>\n</ratecontrol>\n";

  return sa.take_string();
}

RateSelection *
SetTXPowerRate::get_rateselection(uint32_t rateselection_strategy)
{
  return (RateSelection *)_scheme_list.get_scheme(rateselection_strategy);
}

/************************************************************************************************************/
/**************************************** H A N D L E R *****************************************************/
/************************************************************************************************************/

enum {H_INFO};

static String
SetTXPowerRate_read_param(Element *e, void *thunk)
{
  SetTXPowerRate *spr = (SetTXPowerRate *)e;
  switch ((uintptr_t) thunk) {
    case H_INFO: return spr->getInfo();
  }
  return String();
}

void
SetTXPowerRate::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("info", SetTXPowerRate_read_param, (void *) H_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXPowerRate)
