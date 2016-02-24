#include <click/config.h>
#include "dhtstorage.hh"

CLICK_DECLS

DHTStorage::DHTStorage():
  _dht_routing(NULL)
{
}

DHTStorage::~DHTStorage()
{
}

ELEMENT_REQUIRES(BRNElement)
ELEMENT_PROVIDES(DHTStorage)
CLICK_ENDDECLS

