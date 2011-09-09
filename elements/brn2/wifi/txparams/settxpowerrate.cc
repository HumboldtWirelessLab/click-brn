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
  _rate_selection(NULL),
  _cst(NULL),
  _timer(this),
  _packet_size_threshold(0),
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
      "RT", cpkM+cpkP, cpElement, &_rtable,
      "MAXPOWER", cpkM+cpkP, cpInteger, &_max_power,
      "CHANNELSTATS", cpkP, cpElement, &_cst,
      "RATESELECTION", cpkP, cpElement, &_rate_selection,
      "THRESHOLD", cpkP, cpInteger, &_packet_size_threshold,
      "OFFSET", cpkP, cpInteger, &_offset,
      "DEBUG", 0, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}


int
SetTXPowerRate::initialize(ErrorHandler *)
{
  _rate_selection->init(_rtable);

  _timer.initialize(this);

  if ( _rate_selection->get_adjust_period() > 0 ) {
    _timer.schedule_now();
  }
  return 0;
}

void
SetTXPowerRate::run_timer(Timer *)
{
  _timer.schedule_after_msec(_rate_selection->get_adjust_period());
  _rate_selection->adjust_all(&_neighbors);
}

Packet *
SetTXPowerRate::handle_packet( int port, Packet *p )
{
  if ( p == NULL ) return NULL;

  struct click_wifi *wh = (struct click_wifi *) (p->data() + _offset);
  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  NeighbourRateInfo *dsti;

  switch ( port) {
    case 0:                                       //Got packet from upper layer
        {
          dsti = getDstInfo(EtherAddress(wh->i_addr1));
          _rate_selection->assign_rate(ceh, dsti);
          break;
        }
   case 1:                                        // TXFEEDBACK
        {
          EtherAddress dst = EtherAddress(wh->i_addr1);
          bool success = !(ceh->flags & WIFI_EXTRA_TX_FAIL);

          if (!(dst.is_group() || !ceh->rate || (success && p->length() < _packet_size_threshold))) {
            dsti = getDstInfo(dst);  //dst of packet is other node (txfeedback)

            _rate_selection->process_feedback(ceh, dsti);
          }
          break;
        }
  case 2:                                         //received for other nodes
        {
          dsti = getDstInfo(EtherAddress(wh->i_addr2));  //src of packet is other node
          _rate_selection->process_foreign(ceh, dsti);
          break;
        }
  }

  return p;
}

void
SetTXPowerRate::push( int port, Packet *p )
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
  NeighbourRateInfo *di = _neighbors.findp(ea);

  if ( di == NULL ) {
    _neighbors.insert(ea,NeighbourRateInfo(ea, _rtable->lookup(ea), (uint8_t)_max_power));
    di = _neighbors.findp(ea);
  }

  return di;
}

String
SetTXPowerRate::getInfo()
{
  StringAccum sa;

  String rs;

  if ( _rate_selection == NULL ) rs = "n/a";
  else rs = _rate_selection->name();
  sa << "<ratecontrol rateselection=\"" << rs << "\" >\n";
  sa << "\t<neighbours count=\"" << _neighbors.size() << "\" >\n";
  for (NIter iter = _neighbors.begin(); iter.live(); iter++) {
    NeighbourRateInfo nri = iter.value();
    sa << _rate_selection->print_neighbour_info(&nri,2);
  }
  sa << "\t</neighbours>\n</ratecontrol>\n";

  return sa.take_string();
}

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
