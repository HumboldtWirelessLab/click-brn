#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

#include "dart_idcache.hh"

CLICK_DECLS

DartIDCache::DartIDCache()
{
}

DartIDCache::~DartIDCache()
{
}

DartIDCache::IDCacheEntry *
DartIDCache::addEntry(EtherAddress *ea, uint8_t *id, int id_len)
{
  DartIDCache::IDCacheEntry *n = new IDCacheEntry(ea,id,id_len);
  _idcache.push_back(n);

  return n;
}

DartIDCache::IDCacheEntry *
DartIDCache::getEntry(EtherAddress *ea)
{
  for(int i = 0; i < _idcache.size(); i++)
    if ( memcmp(_idcache[i]->_ea.data(), ea->data(), 6) == 0 ) return _idcache[i];

  return NULL;
}

void
DartIDCache::delEntry(EtherAddress *ea)
{
  IDCacheEntry *entry;

  for(int i = 0; i < _idcache.size(); i++) {
    entry = _idcache[i];
    if ( memcmp(entry->_ea.data(), ea->data(), 6) == 0 ) {
      delete entry;
      _idcache.erase(_idcache.begin() + i);
      break;
    }
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DartIDCache)

