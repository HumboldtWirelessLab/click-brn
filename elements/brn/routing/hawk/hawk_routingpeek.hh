#ifndef BRN_HAWK_ROUTINGPEEK_HH
#define BRN_HAWK_ROUTINGPEEK_HH
#include <click/element.hh>
#include "elements/brn/routing/routing_peek.hh"

CLICK_DECLS

class HawkRoutingPeek : public RoutingPeek
{
  public:
    HawkRoutingPeek();
    ~HawkRoutingPeek();

/*ELEMENT*/
    const char *class_name() const  { return "HawkRoutingPeek"; }
    const char *routing_name() const { return "HawkRoutingPeek"; }

    uint32_t get_all_header_len(Packet *packet);
};

CLICK_ENDDECLS
#endif
