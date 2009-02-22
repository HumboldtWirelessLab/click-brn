#ifndef CLICK_DHTROUTING_FALCON_HH
#define CLICK_DHTROUTING_FALCON_HH

#include "elements/brn/dht/md5.h"

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

  private:

    EtherAddress _me;
    BRNLinkStat *_linkstat;

};

CLICK_ENDDECLS
#endif
