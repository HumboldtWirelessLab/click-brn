#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include "settxtries.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

SetTXTries::SetTXTries()
{
}

SetTXTries::~SetTXTries()
{
}

int
SetTXTries::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _offset = 0;
  _tries = WIFI_MAX_RETRIES+1;
  if (cp_va_kparse(conf, this, errh,
		  cpKeywords, 
		  "TRIES", cpUnsigned, "tries", &_tries,
		  "OFFSET", cpUnsigned, "offset", &_offset,
		  cpEnd) < 0) {
    return -1;
  }

  if (_tries < 1) {
	  return errh->error("TRIES must be >= 1");
  }  
  
  return 0;
}

Packet *
SetTXTries::simple_action(Packet *p_in)
{
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p_in->all_user_anno();
  ceh->magic = WIFI_EXTRA_MAGIC;
  ceh->max_tries = _tries;

  return p_in;
}
enum {H_TRIES};

String
SetTXTries::read_handler(Element *e, void *thunk)
{
  SetTXTries *foo = (SetTXTries *)e;
  switch((uintptr_t) thunk) {
  case H_TRIES: return String(foo->_tries) + "\n";
  default:   return "\n";
  }
  
}

int
SetTXTries::write_handler(const String &arg, Element *e,
			 void *vparam, ErrorHandler *errh) 
{
  SetTXTries *f = (SetTXTries *) e;
  String s = cp_uncomment(arg);
  switch((intptr_t)vparam) {
  case H_TRIES: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("tries parameter must be unsigned");
    f->_tries = m;
    break;
  }
  }
  return 0;
}

void
SetTXTries::add_handlers()
{
  add_read_handler("tries", read_handler, (void *) H_TRIES);
  add_write_handler("tries", write_handler, (void *) H_TRIES);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXTries)

