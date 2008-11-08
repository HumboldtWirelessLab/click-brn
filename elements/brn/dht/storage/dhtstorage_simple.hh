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
  class DHTOperationForward
  {
    public:
      void (*_info_func)(void*,DHTOperation*);
      DHTOperation *_operation;
      void *_info_obj;
      Timestamp _time;

      DHTOperationForward()
      {
        _operation = NULL;
        _info_obj = NULL;
      }

      ~DHTOperationForward() {}

      DHTOperationForward(DHTOperation *op, void (*info_func)(void*,DHTOperation*), void *info_obj)
      {
        _operation = op;
        _info_func = info_func;
        _info_obj = info_obj;
        _time = Timestamp::now();
      }
  };

  typedef Vector<DHTOperationForward*> DHTForwardQueue;

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

    BRNDB _db;
  private:

    DHTRouting *_dht_routing;

    DHTForwardQueue _fwd_queue;

    int _debug;

    /*DHT-Functions*/
    int dht_read(DHTOperation *op);
    int dht_write(DHTOperation *op);
    int dht_insert(DHTOperation *op);
    int dht_remove(DHTOperation *op);
    int dht_lock(DHTOperation *op);
    int dht_unlock(DHTOperation *op);

    void handle_dht_operation(DHTOperation *op);

    void add_handlers();

};

CLICK_ENDDECLS
#endif
