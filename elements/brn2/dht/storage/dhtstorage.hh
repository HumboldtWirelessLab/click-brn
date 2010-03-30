#ifndef DHT_STORAGE_HH
#define DHT_STORAGE_HH
/*#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include <click/timer.hh>
#include <click/timestamp.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>
*/
#include "elements/brn2/brnelement.hh"

#include "elements/brn2/dht/routing/dhtrouting.hh"
#include "elements/brn2/dht/storage/dhtoperation.hh"

CLICK_DECLS

class DHTStorage : public BRNElement
{
  public:
    DHTStorage();
    ~DHTStorage();

    DHTRouting *_dht_routing;

    virtual const char *dhtstorage_name() const = 0;
    virtual uint32_t dht_request(DHTOperation *op, void (*info_func)(void*,DHTOperation*), void *info_obj ) = 0;

    bool range_query_support() {
      if ( _dht_routing ) return _dht_routing->range_query_support();
      return false;
    }

    void range_query_min_max_id(uint8_t *min, uint8_t *max) {
      if ( _dht_routing ) return _dht_routing->range_query_min_max_id(min, max);
    };

};

CLICK_ENDDECLS
#endif
