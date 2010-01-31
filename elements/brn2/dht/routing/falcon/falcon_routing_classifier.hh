#ifndef FALCON_ROUTING_CLASSIFIER_HH
#define FALCON_ROUTING_CLASSIFIER_HH
#include <click/element.hh>

CLICK_DECLS

#define FALCON_ROUTING_SUCC_PORT     0
#define FALCON_ROUTING_POSITION_PORT 1
#define FALCON_NETWORKSIZE_PORT      2
#define FALCON_ROUTING_UNKNOWN_PORT  3

class FalconRoutingClassifier : public Element
{
  public:
    FalconRoutingClassifier();
    ~FalconRoutingClassifier();

/*ELEMENT*/
    const char *class_name() const  { return "FalconRoutingClassifier"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/3-4"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

  private:

    int _debug;
};

CLICK_ENDDECLS
#endif
