#ifndef DHT_CLUSTERING_HH
#define DHT_CLUSTERING_HH
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class Clustering : public BRNElement
{
  public:

    Clustering();
    ~Clustering();

    virtual const char *clustering_name() const = 0; //const : function doesn't change the object (members).
                                                     //virtual: späte Bindung
    void init();
    virtual void add_handlers();

    virtual EtherAddress *get_clusterhead() { return &_clusterhead; }
    virtual bool clusterhead_is_me() = 0;

    virtual String clustering_info();

    EtherAddress _clusterhead;
};

CLICK_ENDDECLS
#endif
