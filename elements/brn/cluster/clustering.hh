#ifndef DHT_CLUSTERING_HH
#define DHT_CLUSTERING_HH
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"

CLICK_DECLS

class Clustering : public BRNElement
{
public:
  class Cluster {
  public:
	  Cluster():_cluster_id(0), _clusterhead(), _member() {}

  public:
    uint32_t _cluster_id;
    EtherAddress _clusterhead;
    Vector<EtherAddress> _member;
  };

  typedef Vector<Cluster*> ClusterList;
  typedef ClusterList::const_iterator ClusterListIter;

 protected:
  	  //
  	  //member
  	  //
  	  BRN2LinkStat *_linkstat;
  	  BRN2NodeIdentity *_node_identity;

 public:

    Clustering();
    ~Clustering();

    virtual const char *clustering_name() const = 0; //const : function doesn't change the object (members).
                                                     //virtual: sp�te Bindung
    void init();
    virtual void add_handlers();

    virtual EtherAddress *get_clusterhead() { return &_own_cluster->_clusterhead; }
    virtual bool clusterhead_is_me() = 0;

    virtual String clustering_info();
    void readClusterInfo( EtherAddress node, uint32_t cID, EtherAddress cnode );

    Cluster *_own_cluster;
    ClusterList _known_clusters;
};

CLICK_ENDDECLS
#endif
