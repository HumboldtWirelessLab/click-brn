#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>

#include "settxpowerrate.hh"

CLICK_DECLS

SetTXPowerRate::SetTXPowerRate():
  _rtable(NULL),
  _rate_selection(NULL),
  _rate_selection_strategy(RATESELECTION_NONE),
  _max_power(0),
  _timer(this),
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

  _scheme_list.parse_schemes(this, errh);

  //TODO: use extra function for next steps
  _rate_selection = get_rateselection(_rate_selection_strategy);

  if ( _rate_selection ) {
    if ( _rate_selection->get_adjust_period() > 0 ) {
      _timer.schedule_now();
    }
  }

  if ( _rtable != NULL ) _rate_selection->init(_rtable);

  return 0;
}

void
SetTXPowerRate::run_timer(Timer *)
{
  _timer.schedule_after_msec(_rate_selection->get_adjust_period());
  _rate_selection->adjust_all(&_neighbors);
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
            dsti = getDstInfo(dst);  //dst of packet is other node (txfeedback)

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

NeighbourRateInfo *
SetTXPowerRate::getDstInfo(EtherAddress ea)
{
  NeighbourRateInfo *di = _neighbors.find(ea);

  if ( di == NULL ) {
    _neighbors.insert(ea, new NeighbourRateInfo(ea, _rtable->lookup(ea), (uint8_t)_max_power));
    di = _neighbors.find(ea);
  }

  return di;
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
    sa << _rate_selection->print_neighbour_info(nri, 2);
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
