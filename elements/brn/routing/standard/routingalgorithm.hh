#ifndef ROUTING_ALGORITHM_HH
#define ROUTING_ALGORITHM_HH
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"

CLICK_DECLS

class RoutingAlgorithm : public BRNElement
{
  public:

    RoutingAlgorithm();
    ~RoutingAlgorithm();

    void init();

    virtual const char *routing_algorithm_name() const = 0; //const : function doesn't change the object (members).
                                                            //virtual: sp�te Bindung

    virtual void get_route(EtherAddress src, EtherAddress dst, Vector<EtherAddress> &route, uint32_t *metric) = 0;
    virtual int32_t metric_from_me(EtherAddress dst) = 0;
    virtual int32_t metric_to_me(EtherAddress src) = 0;

     //TODO: user etheraddr instead of BHI
    virtual void add_node(BrnHostInfo *bhi) = 0;
    virtual void remove_node(BrnHostInfo *bhi) = 0;
    
    virtual void update_link(BrnLinkInfo *link) = 0;

    virtual void add_handlers();

    uint32_t _min_link_metric_within_route;

};

CLICK_ENDDECLS
#endif
