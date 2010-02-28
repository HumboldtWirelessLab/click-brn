#ifndef CLICK_DHTROUTING_FALCON_HH
#define CLICK_DHTROUTING_FALCON_HH

#include "elements/brn2/standard/md5.h"
#include "elements/brn2/dht/routing/dhtrouting.hh"
#include "falcon_routingtable.hh"

CLICK_DECLS

#define FALCON_RESPONSIBLE_CHORD   0
#define FALCON_RESPONSIBLE_FORWARD 1

#define FALCON_MAX_REPLICA 7

class DHTRoutingFalcon : public DHTRouting
{
  public:
    DHTRoutingFalcon();
    ~DHTRoutingFalcon();

/*ELEMENT*/
    const char *class_name() const  { return "DHTRoutingFalcon"; }

    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

/*DHTROUTING*/
    const char *dhtrouting_name() const { return "DHTRoutingFalcon"; }

    bool replication_support() const { return true; }
    int max_replication() const { return FALCON_MAX_REPLICA; }
    DHTnode *get_responsibly_node(md5_byte_t *key);
    DHTnode *get_responsibly_replica_node(md5_byte_t *key, int replica_number);

    DHTnode *get_responsibly_node_backward(md5_byte_t *key);
    DHTnode *get_responsibly_node_forward(md5_byte_t *key);

    int update_node(EtherAddress */*ea*/, md5_byte_t */*key*/, int /*keylen*/) { return 0;}

  //private:
    FalconRoutingTable *_frt;
    int _responsible;

    void handle_routing_update_callback(int status);
};

CLICK_ENDDECLS
#endif
