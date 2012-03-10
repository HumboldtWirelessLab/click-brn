/*
 *
 */

#include <click/config.h>//have to be always the first include
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include "elements/brn2/wifi/brnwifi.hh"
#include "setjammer.hh"

CLICK_DECLS

SetJammer::SetJammer() : _jammer(false)
{
}


SetJammer::~SetJammer()
{
}

int SetJammer::initialize(ErrorHandler *)
{
  return 0;
}

int SetJammer::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
    "JAMMER", cpkP, cpBool, &_jammer,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0) return -1;
  return 0;
}

Packet *SetJammer::simple_action(Packet *p)
{
  if (p) {
    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
    ceh->magic = WIFI_EXTRA_MAGIC;
    BrnWifi::setJammer(ceh,_jammer);
  }
  return p;
}

enum {H_JAMMER};

static String SetJammer_read_param(Element *e, void *thunk)
{
  SetJammer *f = (SetJammer *)e;
  switch ((uintptr_t) thunk) {
    case H_JAMMER:
      return  String(f->_jammer);
    default:
      return String();
  }
}


static int SetJammer_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  SetJammer *f = (SetJammer *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_JAMMER:
      bool j;
      if (!cp_bool(s, &j))
        return errh->error("jammer parameter must be boolean");

      f->_jammer = j;
      break;
  }
  return 0;
}


void SetJammer::add_handlers()
{
  BRNElement::add_handlers();//for Debug-Handlers

  add_write_handler("jammer", SetJammer_write_param, H_JAMMER);
  add_write_handler("jammer", SetJammer_write_param, H_JAMMER);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Brn2_SetRTSCTS)
