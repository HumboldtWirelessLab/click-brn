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

static const char *datatype_to_string[] = { "unknown", "void", "char", "int", "void*", "char*" };
static const char *datatype_to_name[] = { "/*unknown*/", "/*void*/", "_char", "_int", "_voidp", "_charp" };

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

int TCC::initialize(ErrorHandler *)
{
  return 0;
}

Packet*
TCC::simple_action(Packet *p)
{
  if ( (p == NULL) || (_tcc_s == NULL) || (click_tcc_simple_action == NULL) ) return p;

  return click_tcc_simple_action(p);
}

void
TCC::set_simple_action_code(String code)
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

    if ( compile_code(_tcc_s, code) != 0 ) {
      _tcc_s = NULL;
      return;
    }

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

int
TCC::compile_code(TCCState *tcc_s, String code) //Vector<<Funcname, func*>> sym_tab
{
  /* if tcclib.h and libtcc1.a are not installed, where can we find them */
  // if (argc == 2 && !memcmp(argv[1], "lib_path=",9))
  //   tcc_set_lib_path(_tcc_s, argv[1]+9);

  /* MUST BE CALLED before any compilation */
  tcc_set_output_type(tcc_s, TCC_OUTPUT_MEMORY);

  uint8_t *str_d = (uint8_t*)code.data();
  for(int i = 0; i < code.length(); i++) {
    if ( str_d[i] == '@' ) str_d[i] = ',';
  }

  BRN_DEBUG("Code:\n%s",code.c_str());

  if (tcc_compile_string(tcc_s, code.c_str()) == -1) {
    BRN_ERROR("compile error");
    return -1;
  }

  /* as a test, we add a symbol that the compiled program can use.
     You may also open a dll with tcc_add_dll() and use symbols from that */
  //tcc_add_symbol(_tcc_s, "add", add);
  tcc_add_symbol(tcc_s, "tcc_packet_resize",(void*)tcc_packet_resize);
  tcc_add_symbol(tcc_s, "tcc_packet_size",(void*)tcc_packet_size);
  tcc_add_symbol(tcc_s, "tcc_packet_data",(void*)tcc_packet_data);
  tcc_add_symbol(tcc_s, "tcc_packet_kill",(void*)tcc_packet_kill);

  /* relocate the code */
  if (tcc_relocate(tcc_s, TCC_RELOCATE_AUTO) < 0) return -1;

  return 0;
}


int
TCC::add_function(String name, String result, Vector<String> args)
{
  if ( _func_map.findp(name) != NULL ) del_function(name);

  TccFunction *tccf = new TccFunction(name, result, args);
  _func_map.insert(name, tccf);

  StringAccum wrapper_code;

  /** ------------ DataType typedef ---------------- */
  wrapper_code << "typedef struct datatype {\n  char type;\n  union {\n";

  for ( int i = 2; i < 6; i++)
    wrapper_code << "    " << datatype_to_string[i] << " " << datatype_to_name[i] << ";\n";

  wrapper_code << "  };\n} DataType;\n\n";

  /** ------------ function def ---------------- */

  wrapper_code << datatype_to_string[tccf->_result.type] << " " << name << "(";
  for ( int i = 0; i < tccf->_args.size(); i++ ) {
    if ( i > 0 ) wrapper_code << ", ";
    wrapper_code << String(datatype_to_string[tccf->_args[i].type]);
  }
  wrapper_code << ");\n\n";

  /** ------------ wrapper def ---------------- */

  wrapper_code <<  "void " << name << "_wrapper(DataType *result, DataType *args) {\n\t"; //header;
  if ( tccf->_result.type != DATATYPE_VOID ) {
    wrapper_code << "result->" << String(datatype_to_name[tccf->_result.type]) << " = ";
  }
  wrapper_code << name << "(";
  for ( int i = 0; i < tccf->_args.size(); i++ ) {
    if ( i > 0 ) wrapper_code << ", ";
    wrapper_code << "args[" << i << "]." << String(datatype_to_name[tccf->_args[i].type]);
  }
  wrapper_code << ");\n}";

  tccf->_c_wrapper_code = wrapper_code.take_string();
  _last_function = name;

  return 0;
}

int
TCC::del_function(String function)
{
  BRN_DEBUG("Del %s", function.c_str());

  if ( _func_map.findp(function) == NULL ) return 0;

  TccFunction *tccf = _func_map.find(function);

  delete tccf;

  _func_map.erase(function);

  return 0;
}

int
TCC::add_code(String function, String code)
{
  if ( _func_map.findp(function) == NULL ) return 0;

  TccFunction *tccf = _func_map.find(function);

  if ( tccf->_tcc_s == NULL ) tccf->_tcc_s = tcc_new();

  tccf->_c_code = code;

  if (compile_code(tccf->_tcc_s, tccf->_c_code) == 0) {
    void* func = (void*)tcc_get_symbol(tccf->_tcc_s, function.c_str());

    if ( tccf->_tcc_wrapper_s == NULL ) tccf->_tcc_wrapper_s = tcc_new();
    tcc_add_symbol(tccf->_tcc_wrapper_s, function.c_str(), func);

    if (compile_code(tccf->_tcc_wrapper_s, tccf->_c_wrapper_code) == 0) {
      tccf->_wrapper_func = (WrapperFunction)tcc_get_symbol(tccf->_tcc_wrapper_s, (function + String("_wrapper")).c_str());
    } else {
      BRN_ERROR("Wrapper error");
    }
  } else {
    BRN_ERROR("Compile error");
  }

  return 0;
}

int
TCC::call_function(String function, Vector<String> args)
{
  BRN_DEBUG("Call %s with Args: %d", function.c_str(),args.size());

  if ( _func_map.findp(function) == NULL ) return 0;
  TccFunction *tccf = _func_map.find(function);

  DataType result;
  DataType *vargs = new DataType[tccf->_args.size()];

  for ( int i = 0; i < tccf->_args.size(); i++ ) {
    switch (tccf->_args[i].type) {
      case DATATYPE_INT:
        cp_integer(args[i], &(vargs[i]._int));
        BRN_DEBUG("add arg %d",vargs[i]._int);
        break;
      case DATATYPE_CHAR:
        cp_integer(args[i], &(vargs[i]._int));
        vargs[i]._char = vargs[i]._int;
        BRN_DEBUG("add arg %c",vargs[i]._char);
        break;
      case DATATYPE_CHAR_POINTER:
        vargs[i]._charp = new char[args[i].length()+1];
        memcpy(vargs[i]._charp, args[i].c_str(), args[i].length()+1);
        BRN_DEBUG("add arg %s",vargs[i]._charp);
        break;
    }
  }

  tccf->_wrapper_func(&result, vargs);

  switch (tccf->_result.type) {
    case DATATYPE_INT:
        _last_result = String(result._int);
        break;
    case DATATYPE_CHAR:
        _last_result = String(result._char);
        break;
    case DATATYPE_CHAR_POINTER:
        _last_result = String(result._charp);
        break;
  }

  for ( int i = 0; i < tccf->_args.size(); i++ ) {
    if (tccf->_args[i].type == DATATYPE_CHAR_POINTER) delete vargs[i]._charp;
  }

  delete[] vargs;
  return 0;
}

String
TCC::stats()
{
  StringAccum sa;

  sa << "<tcc node=\"" << BRN_NODE_NAME << "\" >\n";
  sa << "</tcc>\n";

  return sa.take_string();
}

enum {H_CODE, H_ADD_PROCEDURE, H_DEL_PROCEDURE, H_COMPILE_PROCEDURE, H_CALL_PROCEDURE, H_RESULT};

static String TCC_read_param(Element *e, void *thunk) {
  StringAccum sa;
  TCC *tcc = (TCC *)e;

  switch((uintptr_t) thunk) {
    case H_CODE:   return tcc->stats();
    case H_RESULT: return tcc->_last_result;
  }

  return sa.take_string();
}


static int TCC_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *) {

  TCC *tcc = (TCC *)e;
  String s = cp_uncomment(in_s);

  switch((intptr_t)vparam) {
    case H_CODE:
      tcc->set_simple_action_code(s);
      break;
    case H_ADD_PROCEDURE:  {
      Vector<String> vargs;
      cp_spacevec(s, vargs);

      String result = vargs[0];
      String name = vargs[1];
      vargs.erase(vargs.begin());
      vargs.erase(vargs.begin());

      tcc->add_function(name, result, vargs);
      break;
    }
    case H_COMPILE_PROCEDURE:  {
      tcc->add_code(tcc->_last_function, s);
      break;
    }
    case H_CALL_PROCEDURE:  {
      Vector<String> vargs;
      cp_spacevec(s, vargs);

      String name = vargs[0];
      vargs.erase(vargs.begin());

      tcc->call_function(name, vargs);
      break;
    }
    default:
      break;
  }

  return 0;
}

void TCC::add_handlers() {

  BRNElement::add_handlers();

  add_read_handler("stats", TCC_read_param, (void *)H_CODE);

  add_read_handler("code", TCC_read_param, (void *)H_CODE);
  add_write_handler("code", TCC_write_param, (void *)H_CODE);

  add_write_handler("add", TCC_write_param, (void *)H_ADD_PROCEDURE);
  add_write_handler("del", TCC_write_param, (void *)H_DEL_PROCEDURE);
  add_write_handler("compile", TCC_write_param, (void *)H_COMPILE_PROCEDURE);
  add_write_handler("call", TCC_write_param, (void *)H_CALL_PROCEDURE);

  add_read_handler("result", TCC_read_param, (void *)H_RESULT);

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
