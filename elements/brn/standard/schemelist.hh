#ifndef SCHEMELIST_HH
#define SCHEMELIST_HH
#include <click/element.hh>
#include <click/error.hh>
#include <click/hashtable.hh>
#include <click/vector.hh>

CLICK_DECLS

class Scheme {

 public:

  Scheme(): _default_strategy(0) {}
  virtual ~Scheme() {}

  uint32_t _default_strategy;
  uint32_t _strategy;

  virtual bool handle_strategy(uint32_t strategy);
  virtual uint32_t get_strategy();
  virtual void set_strategy(uint32_t strategy);

};

class SchemeList {

 public:

  SchemeList(): _scheme_array(NULL) {
    _schemes.clear();
    _scheme_elements.clear();
    _scheme_string = "";
    _element_class = "";
  }

  SchemeList(String elem_class): _scheme_array(NULL) {
    _schemes.clear();
    _scheme_elements.clear();
    _scheme_string = "";
    _element_class = elem_class;
  }

  ~SchemeList() {
    if ( _scheme_array != NULL ) delete[] _scheme_array;
    _scheme_array = NULL;
  }

  uint32_t _default_strategy;

  void set_scheme_string(String s_schemes) { _scheme_string = s_schemes; }

  int parse_schemes(Element *e, ErrorHandler* errh) { return parse_schemes(_scheme_string, e, errh); }

  int parse_schemes(String s_schemes, Element *e, ErrorHandler* errh);
  void *get_scheme(uint32_t _strategy);

  Vector<Scheme *> _schemes;
  Vector<Element *> _scheme_elements;

  void **_scheme_array;
  uint32_t _max_scheme_id;
  String _scheme_string;

  String _element_class;
};

CLICK_ENDDECLS
#endif
