#include <click/config.h>
#include "db.hh"

CLICK_DECLS

BRNDB::BRNDB()
{
}

BRNDB::~BRNDB()
{
}

#include <click/vector.cc>
template class Vector<BRNDB::DBrow*>;

CLICK_ENDDECLS
ELEMENT_PROVIDES(BRNDB)
