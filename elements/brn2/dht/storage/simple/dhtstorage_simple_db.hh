#ifndef DHT_STORAGESIMPLE_DB_HH
#define DHT_STORAGESIMPLE_DB_HH

#include <click/element.hh>

#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/storage/db/db.hh"

CLICK_DECLS

#define DEFAULT_LOCKTIME 3600

/**
 * TODO: move "data move on node update"-stuff to seperate element
*/

class DHTStorageSimpleDB : public Element
{
  public:
    DHTStorageSimpleDB();
    ~DHTStorageSimpleDB();

/*ELEMENT*/
    const char *class_name() const  { return "DHTStorageSimpleDB"; }

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    BRNDB _db;

    /*DHT-Functions*/
    int dht_read(DHTOperation *op);
    int dht_write(DHTOperation *op);
    int dht_insert(DHTOperation *op);
    int dht_remove(DHTOperation *op);
    int dht_lock(DHTOperation *op);
    int dht_unlock(DHTOperation *op);

    int handle_dht_operation(DHTOperation *op);

    int _debug;
};

CLICK_ENDDECLS
#endif
