#ifndef TCC_HH
#define TCC_HH

#include <click/confparse.hh>

#include "elements/brn/brnelement.hh"

#include "libtcc.h"

CLICK_DECLS

#define DATATYPE_UNKNOWN       0
#define DATATYPE_VOID          1
#define DATATYPE_CHAR          2
#define DATATYPE_INT           3
#define DATATYPE_VOID_POINTER  4
#define DATATYPE_CHAR_POINTER  5
#define DATATYPE_INT_POINTER   6

#define DATATYPE_COUNT         6

typedef struct datatype {
  int type;
  union {
    char _char;
    int  _int;
    void* _voidp;
    char* _charp;
  };
} DataType;

typedef void (*WrapperFunction)(DataType *, DataType *);

extern const char *datatype_to_string[];

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

    uint32_t _no_calls;

    TccFunction(String name, String result, Vector<String> args): _wrapper_func() {
      String sresult = cp_uncomment(result);

      _name = name;
      _result.type = string2datatype(sresult);

      for ( int i = 0; i < args.size(); i++ ) {
        DataType dt;
        dt.type = string2datatype(args[i]);
        _args.push_back(dt);
      }

      _tcc_s = NULL;
      _tcc_wrapper_s = NULL;

      _no_calls = 0;
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
  String _last_result;

  int add_function(String name, String result, Vector<String> args);
  int del_function(String function);
  int add_code(String function, String code);
  int call_function(String function, Vector<String> args);
  bool have_function(String function);

  static String datatype2string(uint8_t data_type);
  static uint8_t string2datatype(String _string_type);

};

CLICK_ENDDECLS
#endif
