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
  for(int i = 0; i < _idcache.size(); i++) {                         //search
    if ( memcmp( ea->data(), _idcache[i]->_ea.data(), 6 ) == 0 ) {   //if found
      memcpy(_idcache[i]->_nodeid, id, id_len);                           //update id
      _idcache[i]->_id_length = id_len;
      _idcache[i]->_time = Timestamp::now();
      return _idcache[i];
    }
  }

  DartIDCache::IDCacheEntry *n = new IDCacheEntry(ea,id,id_len);
  _idcache.push_back(n);

  return n;
}

DartIDCache::IDCacheEntry *
DartIDCache::getEntry(EtherAddress *ea)
{
  Timestamp now = Timestamp::now();

  for(int i = 0; i < _idcache.size(); i++) {                           //search
    if ( memcmp(_idcache[i]->_ea.data(), ea->data(), 6) == 0 ) {       //if found
      if ( (now - _idcache[i]->_time).msecval() < DARTMAXKEXCACHETIME ) { //check age, if not too old
        return _idcache[i];                                       //give back
      } else {                                                    //too old ?
        delete _idcache[i];
        _idcache.erase(_idcache.begin() + i);                     //delete !!
        return NULL;                                              //return NULL
      }
    }
  }

  return NULL;
}

void
DartIDCache::delEntry(EtherAddress *ea)
{
  for(int i = 0; i < _idcache.size(); i++) {
    IDCacheEntry *entry = _idcache[i];
    if ( memcmp(entry->_ea.data(), ea->data(), 6) == 0 ) {
      delete entry;
      _idcache.erase(_idcache.begin() + i);
      break;
    }
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DartIDCache)

