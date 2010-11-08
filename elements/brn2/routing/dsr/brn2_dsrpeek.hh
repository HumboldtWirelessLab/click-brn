#ifndef BRN_DSR_PEEK_HH
#define BRN_DSR_PEEK_HH
#include <click/element.hh>
#include "elements/brn2/routing/routing_peek.hh"

CLICK_DECLS

class DSRPeek : public RoutingPeek
{
  public:
    DSRPeek();
    ~DSRPeek();

/*ELEMENT*/
    const char *class_name() const  { return "DSRPeek"; }
    const char *routing_name() const { return "DSRPeek"; }

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
