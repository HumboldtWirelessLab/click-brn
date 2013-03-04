#ifndef BRN_DSR_PEEK_HH
#define BRN_DSR_PEEK_HH
#include <click/element.hh>
#include "elements/brn/routing/routing_peek.hh"

CLICK_DECLS

class DSRPeek : public RoutingPeek
{
  public:
    DSRPeek();
    ~DSRPeek();

/*ELEMENT*/
    const char *class_name() const  { return "DSRPeek"; }
    const char *routing_name() const { return "DSRPeek"; }

    uint32_t get_all_header_len(Packet *packet);
};

CLICK_ENDDECLS
#endif
