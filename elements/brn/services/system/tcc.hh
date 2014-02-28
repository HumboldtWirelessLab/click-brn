#ifndef TCC_HH
#define TCC_HH

#include <click/confparse.hh>

#include "elements/brn/brnelement.hh"

#include "libtcc.h"

CLICK_DECLS

#define DATATYPE_UNKNOWN  0
#define DATATYPE_VOID     1
#define DATATYPE_UINT8    2
#define DATATYPE_UINT16   3
#define DATATYPE_UINT32   4

typedef struct datatype {
  uint8_t type;
  union {
    uint8_t _uint8_t;
    uint16_t _uint16_t;
    uint32_t _uint32_t;
  } data;
} DataType;

static uint8_t string_to_datatype(String _string_type) {
  if ( _string_type == "void" ) return DATATYPE_VOID;
  if ( _string_type == "uint8_t" ) return DATATYPE_UINT8;
  if ( _string_type == "uint16_t" ) return DATATYPE_UINT16;
  if ( _string_type == "uint32_t" ) return DATATYPE_UINT32;
  return DATATYPE_UNKNOWN;
}

class TCC : public BRNElement {
 public:

  class TccFunction {
    String _name;
    DataType _result;
    Vector<DataType> _args;

    TCCState *tcc_s;

    TccFunction(String name, String result, String args) {
      String sresult = cp_uncomment(result);

      String sargs = cp_uncomment(args);
      Vector<String> vargs;
      cp_spacevec(sargs, vargs);

      _name = name;
      _result.type = string_to_datatype(sresult);

      for ( int i = 0; i < vargs.size(); i++ ) {
        DataType dt;
        dt.type = string_to_datatype(vargs[i]);
        _args.push_back(dt);
      }
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
  void add_code(String code);
  String stats();

  static void *tcc_packet_resize(void *, int, int);
  static int tcc_packet_size(void *);
  static const uint8_t *tcc_packet_data(void *);
  static void tcc_packet_kill(void *);

 private:

   TCCState *_tcc_s;

   Packet *(*click_tcc_simple_action)(Packet *p);

  
};

CLICK_ENDDECLS
#endif
