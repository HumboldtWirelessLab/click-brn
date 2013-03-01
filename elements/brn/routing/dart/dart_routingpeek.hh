#ifndef BRN_DART_ROUTINGPEEK_HH
#define BRN_DART_ROUTINGPEEK_HH
#include <click/element.hh>
#include "elements/brn/routing/routing_peek.hh"

CLICK_DECLS

class DartRoutingPeek : public RoutingPeek
{
  public:
    DartRoutingPeek();
    ~DartRoutingPeek();

/*ELEMENT*/
    const char *class_name() const  { return "DartRoutingPeek"; }
    const char *routing_name() const { return "DartRoutingPeek"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    void push( int port, Packet *packet );

  private:

    uint32_t get_all_header_len(Packet *packet);
};

CLICK_ENDDECLS
#endif
