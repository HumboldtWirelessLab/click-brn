#ifndef ROUTING_ALGORITHM_HH
#define ROUTING_ALGORITHM_HH
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class RoutingAlgorithm : public BRNElement
{
  public:

    RoutingAlgorithm();
    ~RoutingAlgorithm();

    void init();

    virtual const char *routing_algorithm_name() const = 0; //const : function doesn't change the object (members).
                                                            //virtual: späte Bindung

    //virtual const char *calc_routes() const = 0; //const : function doesn't change the object (members).
    virtual void calc_routes(EtherAddress src, bool from_me) = 0; //const : function doesn't change the object (members).
    virtual void add_handlers();

    uint32_t _min_link_metric_within_route;

};

CLICK_ENDDECLS
#endif
