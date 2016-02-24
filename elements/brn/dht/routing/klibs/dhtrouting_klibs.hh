#ifndef CLICK_DHTROUTING_KLIBS_HH
#define CLICK_DHTROUTING_KLIBS_HH
#include <click/timer.hh>

#include "elements/brn/standard/packetsendbuffer.hh"
#include "elements/brn/standard/brn_md5.hh"

#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"
#include "elements/brn/dht/routing/dhtrouting.hh"
//#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"

CLICK_DECLS

#define ALL_NODES 0
#define OWN_NODES 1
#define FOREIGN_NODES 2

/**
 * TODO: params for size of foreign_list, number of nodes <8foreign and own per packetsendbuffet, max age, max ping
 * TODO: adjust foreignnodelistsize depending on ownnodeslistsize since md5 distribut equally
 * TODO: don't send ping if last ping not so far
*/


class BRN2LinkStat;

class DHTRoutingKlibs : public DHTRouting
{
  public:
    DHTRoutingKlibs();
    ~DHTRoutingKlibs();

/*ELEMENT*/
    const char *class_name() const  { return "DHTRoutingKlibs"; }

    void *cast(const char *name);

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

/*DHTROUTING*/
    const char *dhtrouting_name() const { return "DHTRoutingKlibs"; }
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

    DHTnodelist _own_dhtnodes;
    DHTnodelist _foreign_dhtnodes;

    uint32_t _p_own_dhtnodes;
    uint32_t _p_foreign_dhtnodes;
    int _max_foreign_nodes;

    int _max_own_nodes_per_packet;
    int _max_foreign_nodes_per_packet;

    int _start_time;
    int _update_interval;
    int _max_age;
    int _max_ping_time;

    Timer _lookup_timer;
    Timer _packet_buffer_timer;
    static void static_lookup_timer_hook(Timer *, void *);
    static void static_packet_buffer_timer_hook(Timer *, void *);
    void set_lookup_timer();

    void nodeDetection();

    void handle_hello(Packet *p);
    void handle_request(Packet *p, uint32_t node_group);
    bool update_nodes(DHTnodelist *dhtlist);

    //bool is_foreign(md5_byte_t *key);
    //bool is_foreign(DHTnode *node);
    bool is_own(md5_byte_t *key);
    bool is_own(DHTnode *node);

    int get_nodelist(DHTnodelist *list, DHTnode *_dst, uint32_t group);
    void sendHello();
};

CLICK_ENDDECLS
#endif
