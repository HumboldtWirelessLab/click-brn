#ifndef DARTIDCACHE_HH
#define DARTIDCACHE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn2/dht/standard/dhtnode.hh"

CLICK_DECLS

#define DARTMAXKEXCACHETIME 1000

class DartIDCache : public Element {

 public:

  class IDCacheEntry {
    public:
      int _id_length;
      uint8_t _nodeid[MAX_NODEID_LENTGH];

      EtherAddress _ea;

      Timestamp _time;

      IDCacheEntry( EtherAddress *ea, uint8_t *id, int id_len) {
        _id_length = id_len;
        memcpy(_nodeid, id, MAX_NODEID_LENTGH);
        _ea = EtherAddress(ea->data());
        _time = Timestamp::now();
      }

      ~IDCacheEntry() {};

      void update(DHTnode *n) {
        _id_length = n->_digest_length;
        memcpy(_nodeid,n->_md5_digest, MAX_NODEID_LENTGH);
        _time = Timestamp::now();
      }
  };

  DartIDCache();
  ~DartIDCache();

  const char *class_name() const	{ return "DartIDCache"; }

  Vector<IDCacheEntry*> _idcache;

  DartIDCache::IDCacheEntry *addEntry(EtherAddress *ea, uint8_t *id, int id_len);
  IDCacheEntry *getEntry(EtherAddress *ea);
  void delEntry(EtherAddress *ea);
};

CLICK_ENDDECLS
#endif
