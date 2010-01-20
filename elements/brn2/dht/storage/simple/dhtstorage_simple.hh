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

#include "elements/brn2/dht/routing/dhtrouting.hh"
#include "elements/brn2/dht/storage/dhtstorage.hh"
#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/storage/db/db.hh"

CLICK_DECLS

#define DEFAULT_LOCKTIME 3600

#define DEFAULT_REQUEST_TIMEOUT 1000
#define DEFAULT_MAX_RETRIES 10



class DHTStorageSimple : public DHTStorage
{
  public:
  class DHTOperationForward
  {
    public:
      void (*_info_func)(void*,DHTOperation*);
      DHTOperation *_operation;
      void *_info_obj;

      Timestamp _last_request_time;

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
        op->request_time = Timestamp::now();
        _last_request_time = Timestamp::now();
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

    uint32_t dht_request(DHTOperation *op, void (*info_func)(void*,DHTOperation*), void *info_obj );

    static void req_queue_timer_hook(Timer *, void *);
    void check_queue();

    BRNDB _db;
    DHTRouting *_dht_routing;

    void handle_notify_callback(int status);

    uint32_t handle_node_update();

  private:

    DHTForwardQueue _fwd_queue;

    Timer _check_req_queue_timer;                 //Timer used to check for timeouts

    int _debug;

    /*DHT-Functions*/
    int dht_read(DHTOperation *op);
    int dht_write(DHTOperation *op);
    int dht_insert(DHTOperation *op);
    int dht_remove(DHTOperation *op);
    int dht_lock(DHTOperation *op);
    int dht_unlock(DHTOperation *op);

    int handle_dht_operation(DHTOperation *op);
    uint32_t get_next_dht_id();

    void add_handlers();

    uint32_t _dht_id;

    uint32_t _max_req_time;
    uint32_t _max_req_retries;

    int get_time_to_next();
    bool isFinalTimeout(DHTOperationForward *fwdop);

};

CLICK_ENDDECLS
#endif
