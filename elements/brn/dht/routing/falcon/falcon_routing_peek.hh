#ifndef FALCON_ROUTING_PEEK_HH
#define FALCON_ROUTING_PEEK_HH
#include <click/element.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/routing_peek.hh"

#include "falcon_routingtable.hh"
CLICK_DECLS

class FalconRoutingPeek : public BRNElement
{
  public:
    FalconRoutingPeek();
    ~FalconRoutingPeek();

/*ELEMENT*/
    const char *class_name() const  { return "FalconRoutingPeek"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "0/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);
    void add_handlers();

    bool handle_routing_peek(Packet *p, EtherAddress *src, EtherAddress *dst, int brn_port);

  private:
    FalconRoutingTable *_frt;
    RoutingPeek *_routing_peek;

    bool handle_request(Packet *p);
    void handle_reply(Packet *p);
    void handle_leave(Packet *p);
    void handle_nws(Packet *p);

    bool _reroute_req;
    bool _active;

};

CLICK_ENDDECLS
#endif
