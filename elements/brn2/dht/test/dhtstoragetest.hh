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

#include "elements/brn2/dht/storage/dhtstorage.hh"

#define MODE_INSERT 0
#define MODE_READ   1

CLICK_DECLS

class DHTStorageTest : public Element
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

    DHTStorage *_dht_storage;

    uint32_t _key;
    int _interval;
    uint8_t _mode;

  private:

    Timer _request_timer;

    int _debug;

    int _starttime;

    uint32_t _countkey;

    bool _write;
  public:
    int write_req;
    int write_rep;
    int read_req;
    int read_rep;
};

CLICK_ENDDECLS
#endif
