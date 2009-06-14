#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>

#include "rxpowerstats.hh"

CLICK_DECLS

int
RXPowerStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _debug = false;

  ret = cp_va_kparse(conf, this, errh,
          "DEBUG", cpkP, cpBool, &_debug,
          cpEnd);

  return ret;
}

void
RXPowerStats::push(int port, Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  if ( !(ceh->flags & WIFI_EXTRA_TX) ) {
    struct click_wifi *w = (struct click_wifi *) p->data();
  }

  output(port).push(p);
}

String
RXPowerStats::stats()
{
  StringAccum sa;

  return sa.take_string();
}

void 
RXPowerStats::reset()
{
  _rxinfolist.clear();
}

enum {H_DEBUG, H_STATS, H_RESET};

static String 
RXPowerStats_read_param(Element *e, void *thunk)
{
  RXPowerStats *td = (RXPowerStats *)e;
  switch ((uintptr_t) thunk) {
    case H_DEBUG:
      return String(td->_debug) + "\n";
    case H_STATS:
      return td->stats();
    default:
      return String();
  }
}

static int 
RXPowerStats_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  RXPowerStats *f = (RXPowerStats *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {    //debug
      bool debug;
      if (!cp_bool(s, &debug))
        return errh->error("debug parameter must be boolean");
      f->_debug = debug;
      break;
    }
    case H_RESET: {    //reset
      f->reset();
    }
  }
  return 0;
}

void
RXPowerStats::add_handlers()
{
  add_read_handler("debug", RXPowerStats_read_param, (void *) H_DEBUG);
  add_read_handler("stats", RXPowerStats_read_param, (void *) H_STATS);

  add_write_handler("debug", RXPowerStats_write_param, (void *) H_DEBUG);
  add_write_handler("reset", RXPowerStats_write_param, (void *) H_RESET, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RXPowerStats)
