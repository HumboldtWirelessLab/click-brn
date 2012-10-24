#ifndef DHT_STORAGESIMPLEROUTINGUPDATEHANDLER_HH
#define DHT_STORAGESIMPLEROUTINGUPDATEHANDLER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include <click/timer.hh>
#include <click/timestamp.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>

#include "elements/brn/dht/routing/dhtrouting.hh"

#include "elements/brn/dht/storage/db/db.hh"

CLICK_DECLS

class DHTStorageSimpleRoutingUpdateHandler : public Element
{
 public:

  class DHTMovedDataInfo
  {
   public:
    uint32_t _movedID;
    EtherAddress _target;
    Timestamp _move_time;
    uint32_t _tries;

    DHTMovedDataInfo(EtherAddress *target, uint32_t moveID) {
      _movedID = moveID;
      _target = EtherAddress(target->data());
      _move_time = Timestamp::now();
      _tries = 0;
    }
  };

  typedef Vector<DHTMovedDataInfo*> DHTMovedDataQueue;

  public:
    DHTStorageSimpleRoutingUpdateHandler();
    ~DHTStorageSimpleRoutingUpdateHandler();

    const char *class_name() const  { return "DHTStorageSimpleRoutingUpdateHandler"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

    BRNDB *_db;

    DHTRouting *_dht_routing;

    void handle_notify_callback(int status);

    uint32_t handle_node_update();

    static void data_move_timer_hook(Timer *, void *f);
    void check_moved_data();
    void set_move_timer();

  private:

    int _debug;

    uint32_t _moved_id;
    DHTMovedDataQueue _md_queue;
    DHTMovedDataInfo* get_move_info(EtherAddress *ea);
    void handle_moved_data(Packet *p);
    void handle_ack_for_moved_data(Packet *p);
    Timer _move_data_timer;
};

CLICK_ENDDECLS
#endif
