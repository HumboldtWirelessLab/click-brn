#include <click/config.h>
#include "db.hh"

CLICK_DECLS

BRNDB::BRNDB(Vector<String> /*_col_names*/,Vector<int> /*_col_types*/)
{
  //int i;
  //for( i = 0; i < _col_names.size()
}

BRNDB::~BRNDB()
{
}

#include <click/vector.cc>
template class Vector<BRNDB::DBrow>;
template class Vector<String>;
template class Vector<int>;
template class Vector<char*>;


CLICK_ENDDECLS
ELEMENT_PROVIDES(BRNDB)
