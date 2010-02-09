#ifndef CLICK_DHTROUTING_DART_HH
#define CLICK_DHTROUTING_DART_HH
#include <click/timer.hh>

#include "elements/brn2/standard/md5.h"
#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "elements/brn2/dht/routing/dhtrouting.hh"

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
    DHTnode *get_responsibly_node(md5_byte_t *key);
    DHTnode *get_responsibly_replica_node(md5_byte_t *key, int replica_number);

    DartRoutingTable *_drt;

  private:
    int _debug;

};

CLICK_ENDDECLS
#endif
