#ifndef CLICK_LOADBALANCER_SINK_HH
#define CLICK_LOADBALANCER_SINK_HH


CLICK_DECLS

class LoadBalancerSink : public Element
{
  public:
    class LBClientNat
    {
      public:
        EtherAddress _src_address;    // neighbor Adress
        IPAddress _src_ip;
        IPAddress _dst_ip;
        uint16_t _src_port;
        uint16_t _dst_port;
        uint16_t _type;
        
        IPAddress _new_src_ip;
        uint16_t _new_src_port;

    };

    LoadBalancerSink();
    ~LoadBalancerSink();

/*ELEMENT*/
    const char *class_name() const  { return "LoadBalancerSink"; }

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
    int _debug;

};

CLICK_ENDDECLS
#endif

