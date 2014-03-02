#ifndef TCC_HH
#define TCC_HH

#include <click/confparse.hh>

#include "elements/brn/brnelement.hh"

#include "libtcc.h"

CLICK_DECLS

#define DATATYPE_UNKNOWN      0
#define DATATYPE_VOID         1
#define DATATYPE_CHAR         2
#define DATATYPE_INT          3
#define DATATYPE_VOID_POINTER 4

typedef struct datatype {
  char type;
  union {
    char _char;
    int  _int;
    void* _voidp;
  } data;
} DataType;

typedef void (*WrapperFunction)(DataType *, DataType *);

static uint8_t string_to_datatype(String _string_type) {
  if ( _string_type == "void" ) return DATATYPE_VOID;
  if ( _string_type == "char" ) return DATATYPE_CHAR;
  if ( _string_type == "int" ) return DATATYPE_INT;
  if ( _string_type == "voidp" ) return DATATYPE_VOID_POINTER;
  return DATATYPE_UNKNOWN;
}

static const char *datatype_to_string[] = { "unknown", "void", "char", "int", "void*" };
static const char *datatype_to_name[] = { "/*unknown*/", "/*void*/", "_char", "_int", "_voidp" };

class TCC : public BRNElement {
 public:

  class TccFunction {
   public:
    String _name;
    DataType _result;
    Vector<DataType> _args;

    String _c_wrapper_code;
    String _c_code;

    TCCState *_tcc_wrapper_s;
    TCCState *_tcc_s;

    WrapperFunction _wrapper_func;

    TccFunction(String name, String result, Vector<String> args) {
      String sresult = cp_uncomment(result);

      _name = name;
      _result.type = string_to_datatype(sresult);

      for ( int i = 0; i < args.size(); i++ ) {
        DataType dt;
        dt.type = string_to_datatype(args[i]);
        _args.push_back(dt);
      }

      _tcc_s = NULL;
      _tcc_wrapper_s = NULL;

    }
  };

  typedef HashMap<String, TccFunction*> TccFunctionMap;
  typedef TccFunctionMap::const_iterator TccFunctionMapIter;

  TCC();
  ~TCC();

  const char *class_name() const { return "TCC"; }
  const char *port_count() const { return "0-1/0-1"; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &conf, ErrorHandler *errh);
  bool can_live_reconfigure() const { return false; }
  int initialize(ErrorHandler *);
  void add_handlers();

  Packet* simple_action(Packet *p);
  void set_simple_action_code(String code);
  String stats();

  static void *tcc_packet_resize(void *, int, int);
  static int tcc_packet_size(void *);
  static const uint8_t *tcc_packet_data(void *);
  static void tcc_packet_kill(void *);

 private:

  TCCState *_tcc_s;

  Packet *(*click_tcc_simple_action)(Packet *p);

  /*************************************************************/
  /************ O T H E R   P R O C E D U R E S ****************/
  /*************************************************************/

  int compile_code(TCCState *tcc_s, String code);

 public:

  TccFunctionMap _func_map;
  String _last_function;

  int add_function(String name, String result, Vector<String> args);
  int del_function(String function);
  int add_code(String function, String code);
  int call_function(String function, Vector<String> args);

};

CLICK_ENDDECLS
#endif
