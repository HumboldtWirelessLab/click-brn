#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

#include "dhtstorage_key_cache.hh"

CLICK_DECLS

DHTStorageKeyCache::DHTStorageKeyCache()
{
}

DHTStorageKeyCache::~DHTStorageKeyCache()
{
}

void
DHTStorageKeyCache::addEntry(uint8_t *id, uint8_t replica, uint8_t *ea)
{
  for(int i = 0; i < _idcache.size(); i++) {                      //search
    if ( ( memcmp( id, _idcache[i]._id, MD5_DIGEST_LENGTH) == 0 ) &&
         ( _idcache[i]._replica == replica ) ) {                  //if found
      _idcache[i]._ea = EtherAddress(ea);                         //update id
      _idcache[i]._time = Timestamp::now();
      return;
    }
  }

  _idcache.push_back(IDCacheEntry(id,replica,ea));                       //if not found -> add
}

void
DHTStorageKeyCache::addEntry(uint8_t *id, uint8_t replica, EtherAddress *ea)
{
  addEntry(id,replica,ea->data());
}

EtherAddress *
DHTStorageKeyCache::getEntry(uint8_t *id, uint8_t replica)
{
  Timestamp now = Timestamp::now();

  for(int i = 0; i < _idcache.size(); i++) {                      //search
    if ( ( memcmp( id, _idcache[i]._id, MD5_DIGEST_LENGTH) == 0 ) &&
         ( _idcache[i]._replica == replica ) ) {                   //if found
      if ( (now - _idcache[i]._time).msecval() < MAXKEXCACHETIME ) {//check age, if not too old
        return &(_idcache[i]._ea);                               //give back
      } else {                                                    //too old ?
        _idcache.erase(_idcache.begin() + i);                     //delete !!
        return NULL;                                              //return NULL
      }
    }
  }

  return NULL;
}

void
DHTStorageKeyCache::delEntry(uint8_t *id)
{
  for(int i = 0; i < _idcache.size(); i++) {
    if ( memcmp( id, _idcache[i]._id, MD5_DIGEST_LENGTH) == 0  ) {
      _idcache.erase(_idcache.begin() + i);
    }
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageKeyCache)

