#ifndef DHT_STORAGESIMPLE_HH
#define DHT_STORAGESIMPLE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include <click/timer.hh>
#include <click/timestamp.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>

#include "elements/brn/dht/routing/dhtrouting.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"
#include "elements/brn/dht/storage/dhtoperation.hh"
#include "db.hh"

CLICK_DECLS

class DHTStorageSimple : public DHTStorage
{
  public:
    DHTStorageSimple();
    ~DHTStorageSimple();

    const char *class_name() const  { return "DHTStorageSimple"; }
    void *cast(const char *name);

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    const char *dhtstorage_name() const { return "DHTStorageSimple"; };

    int dht_request(DHTOperation *op, void (*info_func)(void*,DHTOperation*), void *info_obj );

/*DHT-Functions*/
    void handle_dht_operation(DHTOperation *op);

    int dht_read(DHTOperation *op);
    int dht_write(DHTOperation *op);
    int dht_insert(DHTOperation *op);
    int dht_remove(DHTOperation *op);
    int dht_lock(DHTOperation *op);
    int dht_unlock(DHTOperation *op);

    void add_handlers();

  private:

    DHTRouting *_dht_routing;
    BRNDB *_db;
    int _debug;
};

CLICK_ENDDECLS
#endif
