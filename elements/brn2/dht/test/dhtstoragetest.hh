#ifndef DHT_STORAGETEST_HH
#define DHT_STORAGETEST_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include <click/timer.hh>
#include <click/timestamp.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>

#include <elements/brn2/brnelement.hh>

#include "elements/brn2/dht/storage/dhtstorage.hh"

#define MODE_INSERT 0
#define MODE_READ   1


CLICK_DECLS

class DHTStorageTest : public BRNElement
{
  public:
    DHTStorageTest();
    ~DHTStorageTest();

    const char *class_name() const  { return "DHTStorageTest"; }
    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    static void static_request_timer_hook(Timer *, void *);
    void request_timer_hook(Timer *t);
    void request(uint32_t key, uint8_t mode);


    static void callback_func(void *e, DHTOperation *op);
    void callback(DHTOperation *op);

    DHTStorage *_dht_storage;

    uint32_t _key;
    int _interval;
    uint8_t _mode;

  private:

    Timer _request_timer;

    int _starttime;

    uint32_t _startkey;
    uint32_t _countkey;

    bool _write;
    bool _read;

  public:             //public since it is needed in the handler
    int op_rep;
    int write_req;
    int write_rep;
    int read_req;
    int read_rep;
    int not_found;
    int no_timeout;

    uint32_t op_time;
    uint32_t write_time;
    uint32_t read_time;
    uint32_t notfound_time;
    uint32_t timeout_time;
    uint32_t max_timeout_time;

    int _retries;
    int _replica;

    uint32_t last_key;
    bool last_read;
    bool last_timeout;
    bool last_not_found;
    char last_value[10];
};

CLICK_ENDDECLS
#endif
