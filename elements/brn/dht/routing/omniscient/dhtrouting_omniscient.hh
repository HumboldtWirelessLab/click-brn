#ifndef CLICK_DHTROUTING_OMNISCIENT_HH
#define CLICK_DHTROUTING_OMNISCIENT_HH
#include <click/timer.hh>

#include "elements/brn/standard/packetsendbuffer.hh"

#include "elements/brn/dht/md5.h"

#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"
#include "elements/brn/dht/routing/dhtrouting.hh"

CLICK_DECLS

class BRNLinkStat;

class DHTRoutingOmni : public DHTRouting
{
  public:
    DHTRoutingOmni();
    ~DHTRoutingOmni();

/*ELEMENT*/
    const char *class_name() const  { return "DHTRoutingOmni"; }

    void *cast(const char *name);

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

/*DHTROUTING*/
    const char *dhtrouting_name() const { return "DHTRoutingOmni"; }
    bool replication_support() const { return false; }
    int max_replication() const { return(1); }
    DHTnode *get_responsibly_node(md5_byte_t *key);

    String routing_info(void);
    PacketSendBuffer packetBuffer;

  private:

    int _debug;
    BRNLinkStat *_linkstat;

    DHTnodelist _dhtnodes;

    Timer _lookup_timer;
    Timer _packet_buffer_timer;
    static void static_lookup_timer_hook(Timer *, void *);
    static void static_packet_buffer_timer_hook(Timer *, void *);
    void set_lookup_timer();
    void nodeDetection();

    int _update_interval;

    void handle_hello(Packet *p);
    void handle_hello_request(Packet *p);
    void handle_routetable_request(Packet *p);
    void handle_routetable_reply(Packet *p);
    void send_routetable_update(EtherAddress *dst, int status);
    void update_nodes(DHTnodelist *dhtlist);

};

CLICK_ENDDECLS
#endif