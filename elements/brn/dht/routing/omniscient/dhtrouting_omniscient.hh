#ifndef CLICK_DHTROUTING_OMNISCIENT_HH
#define CLICK_DHTROUTING_OMNISCIENT_HH
#include <click/element.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/packetsendbuffer.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "elements/brn2/dht/routing/dhtrouting.hh"


CLICK_DECLS


#define DHT_OMNI_DEFAULT_UPDATE_INTERVAL 1000
#define DHT_OMNI_DEFAULT_START_DELAY     5000
#define DHT_OMNI_MAX_PACKETSIZE_ROUTETABLE 1000

class DHTRoutingOmni : public DHTRouting {
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
    int max_replication() const { return 0; }

  private:
    DHTnode *get_responsibly_node_for_key(md5_byte_t *key);
  public:
    DHTnode *get_responsibly_node(md5_byte_t *key, int replica_number = 0);

    bool range_query_support() { return false; }
    void range_query_min_max_id(uint8_t */*min*/, uint8_t */*max*/) {}

    int update_node(EtherAddress */*ea*/, md5_byte_t */*key*/, int /*keylen*/) { return 0;}

    String routing_info(void);
    PacketSendBuffer packetBuffer;

  private:
    BRN2LinkStat *_linkstat;

    DHTnodelist _dhtnodes;

    Timer _ping_timer;
    Timer _lookup_timer;
    Timer _packet_buffer_timer;

    static void static_ping_timer_hook(Timer *, void *);
    static void static_lookup_timer_hook(Timer *, void *);
    static void static_packet_buffer_timer_hook(Timer *, void *);
    void set_lookup_timer();
    void nodeDetection();
    void ping_timer();

    int _update_interval;
    int _start_delay;

    void handle_hello(Packet *p);
    void handle_hello_request(Packet *p);
    void handle_routetable_request(Packet *p);
    void handle_routetable_reply(Packet *p);

    void send_routetable_request(EtherAddress *dst);
    void send_routetable_update(EtherAddress *dst, int status);

    void update_nodes(DHTnodelist *dhtlist);

};

CLICK_ENDDECLS
#endif
