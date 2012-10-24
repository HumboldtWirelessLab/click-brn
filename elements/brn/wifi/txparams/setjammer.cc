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

#if CLICK_NS
#include <click/router.hh>
#endif

#include "elements/brn/wifi/brnwifi.hh"
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

String
SetJammer::read_cca()
{
  int cca[4];
  cca[0] = 0; //command read
  cca[1] = 0;
  cca[2] = 0;
  cca[3] = 0;

#if CLICK_NS
  simclick_sim_command(router()->simnode(), SIMCLICK_CCA_OPERATION, &cca);
#endif

  StringAccum sa;
  sa << "<cca cs_treshold=\"" << cca[1] << "\" rx_treshold=\"" << cca[2] << "\" cp_treshold=\"" << cca[3] << "\" />\n";
  return sa.take_string();
}

void
#if CLICK_NS
SetJammer::set_cca(int cs_threshold, int rx_threshold, int cp_threshold)
{
  int cca[4];
  cca[0] = 1; //command set
  cca[1] = cs_threshold;
  cca[2] = rx_threshold;
  cca[3] = cp_threshold;

  simclick_sim_command(router()->simnode(), SIMCLICK_CCA_OPERATION, &cca);
#else

SetJammer::set_cca(int /*cs_threshold*/, int /*rx_threshold*/, int /*cp_threshold*/)
{
#endif

}


enum {H_JAMMER,
      H_CCA};

static String SetJammer_read_param(Element *e, void *thunk)
{
  SetJammer *f = (SetJammer *)e;
  switch ((uintptr_t) thunk) {
    case H_JAMMER:
      return  String(f->_jammer);
    case H_CCA:
      return f->read_cca();
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
    case H_CCA:
      Vector<String> args;
      cp_spacevec(s, args);

      int cs,rx,cp;
      if (!cp_integer(args[0], &cs)) return errh->error("cs parameter must be integer");
      if (!cp_integer(args[1], &rx)) return errh->error("rx parameter must be integer");
      if (!cp_integer(args[2], &cp)) return errh->error("cp parameter must be integer");

      f->set_cca(cs,rx,cp);
      break;
  }
  return 0;
}


void SetJammer::add_handlers()
{
  BRNElement::add_handlers();//for Debug-Handlers

  add_write_handler("jammer", SetJammer_write_param, H_JAMMER);
  add_write_handler("cca", SetJammer_write_param, H_CCA);

  add_read_handler("jammer", SetJammer_read_param, H_JAMMER);
  add_read_handler("cca", SetJammer_read_param, H_CCA);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(SetJammer)
