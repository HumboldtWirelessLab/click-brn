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
  _cst(NULL)
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
      "DEBUG", 0, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
SetTXPowerRate::handle_packet( int port, Packet *p )
{
  if ( p == NULL ) return NULL;

  struct click_wifi *wh = (struct click_wifi *) p->data();
  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  DstInfo *dsti;

  switch ( port) {
    case 0:                                       //Got packet from upper layer
        {
          struct power_rate_info pri;

          dsti = getDstInfo(EtherAddress(wh->i_addr1));

          break;
        }
   case 1:                                        // TXFEEDBACK
        {
          dsti = getDstInfo(EtherAddress(wh->i_addr1));  //dst of packet is other node (txfeedback)

          break;
        }
  case 2:                                         //received for other nodes
        {
          dsti = getDstInfo(EtherAddress(wh->i_addr2));  //src of packet is other node

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

SetTXPowerRate::DstInfo *
SetTXPowerRate::getDstInfo(EtherAddress ea)
{
  DstInfo *di = _neighbors.findp(ea);

  if ( di == NULL ) {
    _neighbors.insert(ea,DstInfo(ea, _non_ht_rates, &_ht_rates[0][0], (uint8_t)_max_power));
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

  sa << "</ratecontrol>\n";

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
