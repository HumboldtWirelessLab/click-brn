#ifndef CLICK_DHTROUTING_NOFUDIS_HH
#define CLICK_DHTROUTING_NOFUDIS_HH
#include <click/timer.hh>

#include "elements/brn/standard/packetsendbuffer.hh"
#include "elements/brn/standard/brn_md5.hh"

#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"
#include "elements/brn/dht/routing/dhtrouting.hh"


CLICK_DECLS

class BRN2LinkStat;

class DHTRoutingNoFuDis : public DHTRouting
{
  public:
    DHTRoutingNoFuDis();
    ~DHTRoutingNoFuDis();

/*ELEMENT*/
    const char *class_name() const  { return "DHTRoutingNoFuDis"; }

    void *cast(const char *name);

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

/*DHTROUTING*/
    const char *dhtrouting_name() const { return "DHTRoutingNoFuDis"; }
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

    Timer _lookup_timer;
    Timer _packet_buffer_timer;
    static void static_lookup_timer_hook(Timer *, void *);
    static void static_packet_buffer_timer_hook(Timer *, void *);
    void set_lookup_timer();

    int _update_interval;

};

CLICK_ENDDECLS
#endif
