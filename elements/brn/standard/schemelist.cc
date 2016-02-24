#include <click/config.h>
#include <click/confparse.hh>
#include <click/args.hh>

#include "elements/brn/brn2.h"

#include "schemelist.hh"

CLICK_DECLS


bool
Scheme::handle_strategy(uint32_t strategy) { return _default_strategy == strategy; }

uint32_t
Scheme::get_strategy() { return _default_strategy; }

void
Scheme::set_strategy(uint32_t strategy) { _strategy = strategy;}

int
SchemeList::parse_schemes(String s_schemes, Element *element, ErrorHandler* errh)
{
  //click_chatter("> %s <  > %s <",_element_class.c_str(), s_schemes.c_str());
  assert(_element_class != "");
  assert(s_schemes != "");

  Vector<String> schemes;
  String s = cp_uncomment(s_schemes);

  cp_spacevec(s, schemes);

  _max_scheme_id = 0;

  if ( schemes.size() == 0 ) {
    if ( _scheme_array != NULL ) delete[] _scheme_array;
    _scheme_array = NULL;

    return 0;
  }

  for (uint16_t i = 0; i < schemes.size(); i++) {
    Element *e = cp_element(schemes[i], element, errh, NULL);
    if ( e == NULL ) {
      continue;
    }
    Scheme *_scheme = reinterpret_cast<Scheme*>((e->cast("Scheme")));

    if (!_scheme) {
      return errh->error("Element %s is not a '%s'",schemes[i].c_str(),_element_class.c_str());
    } else {
      //click_chatter("%s : %d  (%d): %p",schemes[i].c_str(), _scheme->get_strategy(), _scheme->_default_strategy, _scheme);
      _schemes.push_back(_scheme);
      _scheme_elements.push_back(e);
      if ( _max_scheme_id < _scheme->get_strategy())
        _max_scheme_id = _scheme->get_strategy();
    }
  }

  //click_chatter("Schemes: %s no: %d max_id: %d",s_schemes.c_str(),(uint32_t)_schemes.size(), _max_scheme_id);

  if ( _scheme_array != NULL ) delete[] _scheme_array;
  _scheme_array = new void*[_max_scheme_id + 1];

  for ( uint32_t i = 0; i <= _max_scheme_id; i++ ) {
    _scheme_array[i] = NULL;
    for ( uint32_t s = 0; s < (uint32_t)_schemes.size(); s++ ) {
      if ( _schemes[s]->handle_strategy(i) ) {
        _scheme_array[i] = _scheme_elements[s]->cast(_element_class.c_str());
        break;
      }
    }
  }

  return 0;
}

void *
SchemeList::get_scheme(uint32_t strategy)
{
  if (strategy > _max_scheme_id ) return NULL;
  return _scheme_array[strategy];
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(SchemeList)
