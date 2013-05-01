#ifndef BRN_FLOODING_ROUTINGPEEK_HH
#define BRN_FLOODING_ROUTINGPEEK_HH
#include <click/element.hh>
#include "elements/brn/routing/routing_peek.hh"

CLICK_DECLS

class FloodingRoutingPeek : public RoutingPeek
{
  public:
    FloodingRoutingPeek();
    ~FloodingRoutingPeek();

/*ELEMENT*/
    const char *class_name() const  { return "FloodingRoutingPeek"; }
    const char *routing_name() const { return "FloodingRoutingPeek"; }

    uint32_t get_all_header_len(Packet *packet);
};

CLICK_ENDDECLS
#endif
