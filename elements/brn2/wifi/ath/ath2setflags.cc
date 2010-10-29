#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>

#include "elements/wifi/athdesc.h"

#include "ath2setflags.hh"
#include "ath2_desc.h"

CLICK_DECLS


Ath2SetFlags::Ath2SetFlags():
  _queue(-1)
{
}

Ath2SetFlags::~Ath2SetFlags()
{
}

int
Ath2SetFlags::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_kparse(conf, this, errh,
      "QUEUE", cpkN, cpInteger, &_queue,
      "DEBUG", 0, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
Ath2SetFlags::simple_action(Packet *p)
{
  struct ar5212_desc *desc  = (struct ar5212_desc *) (p->data() + 8);

  struct ath2_header *ath2_h = (struct ath2_header*)&(p->data()[ATHDESC_HEADER_SIZE]);
  if ( _queue != -1 ) ath2_h->anno.tx_anno.queue = _queue;

  return p;
}


enum {H_DEBUG};

static String 
Ath2SetFlags_read_param(Element *e, void *thunk)
{
  Ath2SetFlags *td = (Ath2SetFlags *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
        return String(td->_debug) + "\n";
      default:
        return String();
    }
}
static int 
Ath2SetFlags_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  Ath2SetFlags *f = (Ath2SetFlags *)e;
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
Ath2SetFlags::add_handlers()
{
  add_read_handler("debug", Ath2SetFlags_read_param, (void *) H_DEBUG);
  add_write_handler("debug", Ath2SetFlags_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(Ath2SetFlags)
