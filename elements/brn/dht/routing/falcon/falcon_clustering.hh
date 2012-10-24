#ifndef FALCON_CLUSTERING_HH
#define FALCON_CLUSTERING_HH
#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/cluster/clustering.hh"

#include "falcon_routingtable.hh"
CLICK_DECLS

class FalconClustering : public BRNElement
{
  public:
    FalconClustering();
    ~FalconClustering();

/*ELEMENT*/
    const char *class_name() const  { return "FalconClustering"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);
    void add_handlers();

    void push( int port, Packet *packet );

  private:
    FalconRoutingTable *_frt;
    Clustering *_clustering;

    Timer _lookup_timer;
    static void static_lookup_timer_hook(Timer *, void *);

};

CLICK_ENDDECLS
#endif
