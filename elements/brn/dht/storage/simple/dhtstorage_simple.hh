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

#include "elements/brn/dht/storage/db/db.hh"
#include "elements/brn/dht/storage/db/dhtoperationhandler.hh"

#include "dhtstorage_key_cache.hh"

CLICK_DECLS

#define DHT_STORAGE_STATS

#define DEFAULT_LOCKTIME 3600

#define DEFAULT_REQUEST_TIMEOUT 1000
#define DEFAULT_MAX_RETRIES 10

class DHTStorageSimple : public DHTStorage
{
  public:

  class ReplicaInfo
  {
    public:
      uint8_t replica_number;
      uint16_t value_len;
      uint8_t *value;
      uint8_t status;

      void init(uint8_t replica, uint8_t *val, uint16_t val_len)
      {
        replica_number = replica;
        value_len = val_len;
        value = val;
      }

      ReplicaInfo()
      {
        init(0, NULL, 0);
      }

      ReplicaInfo(int replica)
      {
        init(replica, NULL, 0);
      }

      ReplicaInfo(uint8_t replica, uint8_t *val, uint16_t val_len)
      {
        init(replica, val, val_len);
      }

      void set_value(uint8_t *val, uint16_t val_len)
      {
        value = new uint8_t[val_len];
        memcpy(value, val, val_len);
        value_len = val_len;
      }

      ~ReplicaInfo() {
        if ( value != NULL ) delete[] value;
      }
  };

  class DHTOperationForward
  {
    public:
      void (*_info_func)(void*,DHTOperation*);
      void *_info_obj;

      DHTOperation *_operation;

      Timestamp _last_request_time;

      uint32_t replica_count;
      ReplicaInfo *replicaList;
      uint32_t received_replica_bitmap;

      DHTOperationForward()
      {
        _operation = NULL;
        _info_obj = NULL;
        replicaList = NULL;
        replica_count = 0;
      }

      ~DHTOperationForward()
      {
        if ( replicaList != NULL )
          delete[] replicaList;
      }

      DHTOperationForward(DHTOperation *op, void (*info_func)(void*,DHTOperation*), void *info_obj, int replica)
      {
        _operation = op;
        _info_func = info_func;
        _info_obj = info_obj;
        op->request_time = Timestamp::now();
        _last_request_time = Timestamp::now();
        received_replica_bitmap = 0;
        replicaList = new ReplicaInfo[replica + 1];
        replica_count = replica;
      }

      void set_replica_reply(int replica) {
        received_replica_bitmap |= ( 1 << replica);
      }

      bool have_all_replicas() {
        return ( (uint32_t)(( 1 << (replica_count + 1)) - 1) == received_replica_bitmap );
      }

      bool have_replica(int replica) {
        return ( (received_replica_bitmap & (1 << replica)) != 0 );
      }
  };

  typedef Vector<DHTOperationForward*> DHTForwardQueue;

  public:
    DHTStorageSimple();
    ~DHTStorageSimple();

    const char *class_name() const  { return "DHTStorageSimple"; }
    void *cast(const char *name);

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "0-1/0-1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

    const char *dhtstorage_name() const { return "DHTStorageSimple"; };

    uint32_t dht_request(DHTOperation *op, void (*info_func)(void*,DHTOperation*), void *info_obj );

    static void req_queue_timer_hook(Timer *, void *);
    void check_queue();

    BRNDB *_db;
    DHTOperationHandler *_dht_op_handler;

    DHTStorageKeyCache *_dht_key_cache;

  private:

    DHTForwardQueue _fwd_queue;

    Timer _check_req_queue_timer;                 //Timer used to check for timeouts

    uint16_t get_next_dht_id();
    uint16_t _dht_id;

    uint32_t _max_req_time;
    uint32_t _max_req_retries;

    int get_time_to_next();
    bool isFinalTimeout(DHTOperationForward *fwdop);

    bool _add_node_id; //NB: whether the Node id of the src of a request is add to the request. SHould this be default ? ID to DHTHeader ??

    /* Stats */
#ifdef DHT_STORAGE_STATS
    int _stats_requests;
    int _stats_replies;
    int _stats_retries;
    int _stats_timeouts;
    int _stats_excessive_timeouts;
    int _stats_cache_hits;
    int _stats_hops_sum;

  public:
    String read_stats();
#endif
};

CLICK_ENDDECLS
#endif
