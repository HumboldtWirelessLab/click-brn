#ifndef DHTSTORAGEKEYCACHE_HH
#define DHTSTORAGEKEYCACHE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn2/standard/md5.h"

CLICK_DECLS

#define MAXKEXCACHETIME 20000

class DHTStorageKeyCache : public Element {

 public:

  class IDCacheEntry {
    public:
      uint8_t _id[MD5_DIGEST_LENGTH];
      uint8_t _replica;

      EtherAddress _ea;

      Timestamp _time;

      IDCacheEntry( uint8_t *id, uint8_t replica, uint8_t *ea) {
        memcpy(_id, id, MD5_DIGEST_LENGTH);
        _ea = EtherAddress(ea);
        _time = Timestamp::now();
        _replica = replica;
      }

      ~IDCacheEntry() {};
  };

  DHTStorageKeyCache();
  ~DHTStorageKeyCache();

  const char *class_name() const	{ return "DHTStorageKeyCache"; }

  Vector<IDCacheEntry> _idcache;

  void addEntry(uint8_t *id, uint8_t replica,uint8_t *ea);
  void addEntry(uint8_t *id, uint8_t replica, EtherAddress *ea);
  EtherAddress *getEntry(uint8_t *id, uint8_t replica);
  void delEntry(uint8_t *id);
};

CLICK_ENDDECLS
#endif
