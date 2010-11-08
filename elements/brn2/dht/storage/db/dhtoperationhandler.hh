#ifndef DHT_OPERATION_HANDLER_HH
#define DHT_OPERATION_HANDLER_HH

#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/storage/db/db.hh"

CLICK_DECLS

#define DEFAULT_LOCKTIME 3600

class DHTOperationHandler
{
  public:

    DHTOperationHandler(BRNDB *db);
    DHTOperationHandler(BRNDB *db, int debug);
    ~DHTOperationHandler();

    BRNDB *_db;

    /*DHT-Functions*/
    int dht_test(DHTOperation *op);
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
