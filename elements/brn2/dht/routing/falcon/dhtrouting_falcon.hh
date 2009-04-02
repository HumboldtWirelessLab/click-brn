#ifndef CLICK_DHTROUTING_FALCON_HH
#define CLICK_DHTROUTING_FALCON_HH

#include "elements/brn/dht/md5.h"
#include "elements/brn2/standard/packetsendbuffer.hh"

#include "elements/brn2/dht/routing/dhtrouting.hh"

CLICK_DECLS

class BRNLinkStat;

class DHTRoutingFalcon : public DHTRouting
{
  public:
    DHTRoutingFalcon();
    ~DHTRoutingFalcon();

/*ELEMENT*/
    const char *class_name() const  { return "DHTRoutingFalcon"; }

    void *cast(const char *name);

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

/*DHTROUTING*/
    const char *dhtrouting_name() const { return "DHTRoutingFalcon"; }

    bool replication_support() const { return false; }
    int max_replication() const { return(1); }
    DHTnode *get_responsibly_node(md5_byte_t *key);

    PacketSendBuffer packetBuffer;
    Timer _lookup_timer;
    Timer _packet_buffer_timer;

    static void static_lookup_timer_hook(Timer *, void *);
    static void static_packet_buffer_timer_hook(Timer *, void *);
    void set_lookup_timer();

    void nodeDetection();

    int _update_interval;
    String routing_info(void);

    int _max_fingertable_size;     //!! size^2 = max_number of nodes in network !!
  private:

    BRNLinkStat *_linkstat;
    DHTnodelist _fingertable;

    DHTnode *successor;
    DHTnode *predecessor;


};

CLICK_ENDDECLS
#endif
