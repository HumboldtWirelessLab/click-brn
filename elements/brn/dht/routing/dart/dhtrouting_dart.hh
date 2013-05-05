#ifndef CLICK_DHTROUTING_DART_HH
#define CLICK_DHTROUTING_DART_HH
#include <click/timer.hh>

#include "elements/brn/standard/brn_md5.hh"
#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"
#include "elements/brn/dht/routing/dhtrouting.hh"

#include "dart_routingtable.hh"

CLICK_DECLS

class BRN2LinkStat;

class DHTRoutingDart : public DHTRouting
{
  public:
    DHTRoutingDart();
    ~DHTRoutingDart();

/*ELEMENT*/
    const char *class_name() const  { return "DHTRoutingDart"; }

    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

/*DHTROUTING*/
    const char *dhtrouting_name() const { return "DHTRoutingDart"; }
    bool replication_support() const { return false; }
    int max_replication() const { return 0; }

  private:
    DHTnode *get_responsibly_node_for_key(md5_byte_t *key);
    DHTnode *get_responsibly_node_for_key_opt(md5_byte_t *key);
  public:
    DHTnode *get_responsibly_node(md5_byte_t *key, int replica_number = 0);
    DHTnode *get_responsibly_node_opt(md5_byte_t *key, int replica_number = 0);

    bool range_query_support() { return false; }
    void range_query_min_max_id(uint8_t */*min*/, uint8_t */*max*/) {}

    int update_node(EtherAddress *ea, md5_byte_t *key, int keylen);

    DartRoutingTable *_drt;
    
    bool _expand_neighbourhood; //OPTIMIZATION

    static void routingtable_callback_func(void *e, int status);

};

CLICK_ENDDECLS
#endif
