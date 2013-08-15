#ifndef BRN_DSRHANDLEFEEDBACK_HH
#define BRN_DSRHANDLEFEEDBACK_HH
#include <click/element.hh>
#include <click/hashmap.hh>
#include <click/pair.hh>
#include <click/vector.hh>

#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

CLICK_DECLS

/* PORTS
 * Input:
 * 0 feedback (success)
 * 1 feedback (error)
 * 
 * Output:
 * 0 feedback for dsr
 * 1 feedback for upper layer
 * 
 */

class DSRHandleFeedback : public BRNElement
{

  public:
    DSRHandleFeedback();
    ~DSRHandleFeedback();

/*ELEMENT*/
    const char *class_name() const  { return "DSRHandleFeedback"; }
    const char *routing_name() const { return "DSRHandleFeedback"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "2/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    void push( int port, Packet *packet );

    String read_stats();

private:
    BRN2NodeIdentity *_me;
};

CLICK_ENDDECLS
#endif
