#ifndef DHT_CLUSTERING_HH
#define DHT_CLUSTERING_HH
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

class Clustering : public BRNElement
{
  class Cluster {

    uint32_t _cluster_id;
    EtherAddress _clusterhead;
    Vector<EtherAddress> _member;

  };

  typedef Vector<Cluster*> ClusterList;
  typedef ClusterList::const_iterator ClusterListIter;

  public:

    Clustering();
    ~Clustering();

    virtual const char *clustering_name() const = 0; //const : function doesn't change the object (members).
                                                     //virtual: sp�te Bindung
    void init();
    virtual void add_handlers();

    virtual EtherAddress *get_clusterhead() { return &_clusterhead; }
    virtual bool clusterhead_is_me() = 0;

    virtual String clustering_info();

    EtherAddress _clusterhead; //TODO: remove, it's old stuff

    Cluster _own_cluster;
    ClusterList _known_clusters;
};

CLICK_ENDDECLS
#endif
