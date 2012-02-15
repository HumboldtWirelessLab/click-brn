#ifndef ROUTING_PEEK_HH
#define ROUTING_PEEK_HH
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


    virtual void add_handlers();

};

CLICK_ENDDECLS
#endif
