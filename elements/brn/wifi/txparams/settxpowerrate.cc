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
  _scheme_array(NULL),
  _cst(NULL),
  _timer(this),
  _offset(0)
{
  BRNElement::init();
}

SetTXPowerRate::~SetTXPowerRate()
{
}

int
SetTXPowerRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "RATESELECTIONS", cpkM+cpkP, cpString, &_rate_selection_string,
      "STRATEGY", cpkP, cpInteger, &_rate_selection_strategy,
      "RT", cpkP, cpElement, &_rtable,
      "CHANNELSTATS", cpkP, cpElement, &_cst,
      "OFFSET", cpkP, cpInteger, &_offset,
      "DEBUG", 0, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}


int
SetTXPowerRate::initialize(ErrorHandler *errh)
{
  _timer.initialize(this);

  parse_schemes(_rate_selection_string, errh);

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

  struct click_wifi *wh = (struct click_wifi *) (p->data() + _offset);
  struct brn_click_wifi_extra_extention *wee = BrnWifi::get_brn_click_wifi_extra_extention(p);

  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  NeighbourRateInfo *dsti;

  switch (port) {
    case 0:                                       //Got packet from upper layer
        {
          dsti = getDstInfo(EtherAddress(wh->i_addr1));
          _rate_selection->assign_rate(ceh, wee, dsti);
          break;
        }
   case 1:                                        // TXFEEDBACK
        {
          EtherAddress dst = EtherAddress(wh->i_addr1);

          if (!dst.is_group()) {
            dsti = getDstInfo(dst);  //dst of packet is other node (txfeedback)

            _rate_selection->process_feedback(ceh, wee, dsti);
          }
          break;
        }
  case 2:                                         //received for other nodes
        {
          dsti = getDstInfo(EtherAddress(wh->i_addr2));  //src of packet is other node
          _rate_selection->process_foreign(ceh, wee, dsti);
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

/************************************************************************************************************/
/**************************************** S C H E M E P A R S E R *******************************************/
/************************************************************************************************************/

int
SetTXPowerRate::parse_schemes(String s_schemes, ErrorHandler* errh)
{
  Vector<String> schemes;
  String s = cp_uncomment(s_schemes);

  cp_spacevec(s, schemes);

  _max_scheme_id = 0;

  if ( schemes.size() == 0 ) {
    if ( _scheme_array != NULL ) delete[] _scheme_array;
    _scheme_array = NULL;

    return 0;
  }

  for (uint16_t i = 0; i < schemes.size(); i++) {
    Element *e = cp_element(schemes[i], this, errh);
    RateSelection *rateselection = (RateSelection *)e->cast("RateSelection");

    if (!rateselection) {
      return errh->error("Element %s is not a 'RateSelection'",schemes[i].c_str());
    } else {
      _schemes.push_back(rateselection);
      if ( _max_scheme_id < rateselection->get_strategy())
        _max_scheme_id = rateselection->get_strategy();
    }
  }

  if ( _scheme_array != NULL ) delete[] _scheme_array;
  _scheme_array = new RateSelection*[_max_scheme_id + 1];

  for ( uint32_t i = 0; i <= _max_scheme_id; i++ ) {
    _scheme_array[i] = NULL; //Default
    for ( uint32_t s = 0; s < (uint32_t)_schemes.size(); s++ ) {
      if ( _schemes[s]->handle_strategy(i) ) {
        _scheme_array[i] = _schemes[s];
        break;
      }
    }
  }

  return 0;
}

RateSelection *
SetTXPowerRate::get_rateselection(uint32_t rateselection_strategy)
{
  if ( rateselection_strategy > _max_scheme_id ) return NULL;
  return _scheme_array[rateselection_strategy];
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
