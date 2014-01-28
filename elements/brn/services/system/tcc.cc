//Last modified: 04/04/11 12:34:52(CEST) by Fabian Holler
#include <click/config.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

#include "tcc.hh"

CLICK_DECLS


TCC::TCC():
  _tcc_s(NULL),
  click_tcc_simple_action(NULL)
{
  BRNElement::init();
}

TCC::~TCC() {
}

int
TCC::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
    "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
    cpEnd) < 0)
    return -1;

  return 0;
}

int TCC::initialize() {

  return 0;
}

Packet*
TCC::simple_action(Packet *p)
{
  if ( (p == NULL) || (_tcc_s == NULL) || (click_tcc_simple_action == NULL) ) return p;

  return click_tcc_simple_action(p);
}

void
TCC::add_code(String code)
{
    int (*click_tcc_init)();
    int (*click_tcc_close)();

    if ( _tcc_s != NULL ) {
      click_tcc_close = (int(*)())tcc_get_symbol(_tcc_s, "click_tcc_close");

      if (click_tcc_close) click_tcc_close();

      /* delete the state */
      tcc_delete(_tcc_s);
    }

    _tcc_s = tcc_new();

    if (!_tcc_s) {
        BRN_ERROR("Could not create tcc state\n");
    }

    /* if tcclib.h and libtcc1.a are not installed, where can we find them */
    // if (argc == 2 && !memcmp(argv[1], "lib_path=",9))
    //   tcc_set_lib_path(_tcc_s, argv[1]+9);

    /* MUST BE CALLED before any compilation */
    tcc_set_output_type(_tcc_s, TCC_OUTPUT_MEMORY);

    BRN_ERROR("Code: %s",code.c_str());

    uint8_t *str_d = (uint8_t*)code.data();
    for(int i = 0; i < code.length(); i++) {
      if ( str_d[i] == '@' ) str_d[i] = ',';
    }

    if (tcc_compile_string(_tcc_s, code.data()) == -1) {
      _tcc_s = NULL;
      BRN_ERROR("compile error");
      return;
    }

    /* as a test, we add a symbol that the compiled program can use.
       You may also open a dll with tcc_add_dll() and use symbols from that */
    //tcc_add_symbol(_tcc_s, "add", add);
    tcc_add_symbol(_tcc_s, "tcc_packet_resize",(void*)tcc_packet_resize);
    tcc_add_symbol(_tcc_s, "tcc_packet_size",(void*)tcc_packet_size);
    tcc_add_symbol(_tcc_s, "tcc_packet_data",(void*)tcc_packet_data);
    tcc_add_symbol(_tcc_s, "tcc_packet_kill",(void*)tcc_packet_kill);

    /* relocate the code */
    if (tcc_relocate(_tcc_s, TCC_RELOCATE_AUTO) < 0)
        return;

    /* get entry symbol */
    click_tcc_init = (int(*)())tcc_get_symbol(_tcc_s, "click_tcc_init");

    if (!click_tcc_init) {
      _tcc_s = NULL;
      return;
    }

    /* run the code */
    int result = click_tcc_init();
    BRN_ERROR("Init: %d",result);

    click_tcc_simple_action = (Packet*(*)(Packet *))tcc_get_symbol(_tcc_s, "click_tcc_simple_action");

    if (!click_tcc_simple_action) {
      BRN_ERROR("no simple_action");
    }

    return;
}

String
TCC::stats()
{
  StringAccum sa;

  sa << "<tcc node=\"" << BRN_NODE_NAME << "\" >\n";
  sa << "</tcc>\n";

  return sa.take_string();
}

enum {H_CODE};


static String TCC_read_param(Element *e, void *thunk) {
  StringAccum sa;
  TCC *tcc = (TCC *)e;

  switch((uintptr_t) thunk) {
    case H_CODE:
      return tcc->stats();
    break;
  default:
    break;
  }

  return sa.take_string();
}


static int TCC_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *) {

  TCC *tcc = (TCC *)e;
  String s = cp_uncomment(in_s);

  switch((intptr_t)vparam) {
   case H_CODE:
    tcc->add_code(s);
    break;
  default:
    break;
  }

  return 0;
}

void TCC::add_handlers() {

  BRNElement::add_handlers();

  add_write_handler("code", TCC_write_param, (void *)H_CODE);
  add_read_handler("code", TCC_read_param, (void *)H_CODE);
}

void *TCC::tcc_packet_resize(void *p, int /*h*/, int /*t*/) {
  return p;
}

int TCC::tcc_packet_size(void *p) {
  return ((Packet*)p)->length();
}

const uint8_t *TCC::tcc_packet_data(void *p) {
  return ((Packet*)p)->data();
}

void TCC::tcc_packet_kill(void *p) {
  return ((Packet*)p)->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCC)
