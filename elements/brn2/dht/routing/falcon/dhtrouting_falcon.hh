#ifndef CLICK_DHTROUTING_FALCON_HH
#define CLICK_DHTROUTING_FALCON_HH

#include "elements/brn2/standard/md5.h"
#include "elements/brn2/dht/routing/dhtrouting.hh"
#include "falcon_routingtable.hh"

CLICK_DECLS

#define FALCON_RESPONSIBLE_CHORD   0
#define FALCON_RESPONSIBLE_FORWARD 1

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

    bool replication_support() const { return false; }
    int max_replication() const { return(1); }
    DHTnode *get_responsibly_node(md5_byte_t *key);

    DHTnode *get_responsibly_node_backward(md5_byte_t *key);
    DHTnode *get_responsibly_node_forward(md5_byte_t *key);

  //private:
    FalconRoutingTable *_frt;
};

CLICK_ENDDECLS
#endif
