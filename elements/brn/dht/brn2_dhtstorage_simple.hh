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

#include "brn2_dhtrouting.hh"
#include "brn2_db.hh"

CLICK_DECLS

//class DHTRouting;

class DHTStorageSimple : public Element
{
  public:
    DHTStorageSimple();
    ~DHTStorageSimple();

    const char *class_name() const  { return "DHTStorageSimple"; }
    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

/*DHT-Functions*/
    void dht_read();
    void dht_write();
    void dht_delete();
    void dht_look();
    void dht_unlook();

  private:
    class ForwardInfo {
      public:
        uint8_t  _sender;
        uint8_t  _id;

        struct timeval _send_time;

        Packet *_fwd_packet;
        EtherAddress *_ether_add;

        uint8_t _retry;
 
        ForwardInfo( uint8_t sender, uint8_t id, EtherAddress *rcv_node, Packet *fwd_packet )
        {
          _sender = sender;
          _id = id;
          _send_time = Timestamp::now().timeval();
     
          _retry = 0;
  
          _ether_add = new EtherAddress( rcv_node->data() );

          _fwd_packet = fwd_packet;
        }

        ~ForwardInfo()
        {
        }

    };

    Vector<ForwardInfo> forward_list;

    DHTRouting *_dht_routing;
    BRNDB *_db;
    int _debug;
};

CLICK_ENDDECLS
#endif
