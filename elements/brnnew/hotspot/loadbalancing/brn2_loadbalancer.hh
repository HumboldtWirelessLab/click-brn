#ifndef CLICK_LOADBALANCER_HH
#define CLICK_LOADBALANCER_HH

#include "elements/brn/brnlinkstat.hh"

CLICK_DECLS

class LoadBalancer : public Element
{
  public:
    class LBClient
    {
      public:
        EtherAddress _src_address;
        IPAddress _src_ip;
        IPAddress _dst_ip;
        uint16_t _src_port;
        uint16_t _dst_port;
        uint16_t _type;
    };

    LoadBalancer();
    ~LoadBalancer();

/*ELEMENT*/
    const char *class_name() const  { return "LoadBalancer"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "2/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

    void nodeDetection();

  private:

    EtherAddress _me;
    BRNLinkStat *_linkstat;
    Vector<EtherAddress> _neighbors;
    int _debug;

};

CLICK_ENDDECLS
#endif
