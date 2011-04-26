#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include "settxrates.hh"

CLICK_DECLS


SetTXRates::SetTXRates():
    _rate0(0),
    _rate1(0),
    _rate2(0),
    _rate3(0),
#ifdef CLICK_NS
    _tries0(2),
#else
    _tries0(1),
#endif
    _tries1(0),
    _tries2(0),
    _tries3(0)
{
}

SetTXRates::~SetTXRates()
{
}

int
SetTXRates::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_kparse(conf, this, errh,
      "RATE0", cpkN, cpInteger, &_rate0,
      "RATE1", cpkN, cpInteger, &_rate1,
      "RATE2", cpkN, cpInteger, &_rate2,
      "RATE3", cpkN, cpInteger, &_rate3,
      "TRIES0", cpkN, cpInteger, &_tries0,
      "TRIES1", cpkN, cpInteger, &_tries1,
      "TRIES2", cpkN, cpInteger, &_tries2,
      "TRIES3", cpkN, cpInteger, &_tries3,
      "DEBUG", 0, cpBool, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
SetTXRates::simple_action(Packet *p)
{
  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  ceh->magic = WIFI_EXTRA_MAGIC;

  ceh->rate = _rate0 ? _rate0 : 2;
  ceh->rate1 = _rate1;
  ceh->rate2 = _rate2;
  ceh->rate3 = _rate3;

  ceh->max_tries = _tries0;
  ceh->max_tries1 = _tries1;
  ceh->max_tries2 = _tries2;
  ceh->max_tries3 = _tries3;

  return p;
}


enum {H_DEBUG};

static String 
SetTXRates_read_param(Element *e, void *thunk)
{
  SetTXRates *td = (SetTXRates *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
        return String(td->_debug) + "\n";
      default:
        return String();
    }
}
static int 
SetTXRates_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  SetTXRates *f = (SetTXRates *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
SetTXRates::add_handlers()
{
  add_read_handler("debug", SetTXRates_read_param, (void *) H_DEBUG);
  add_write_handler("debug", SetTXRates_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXRates)
