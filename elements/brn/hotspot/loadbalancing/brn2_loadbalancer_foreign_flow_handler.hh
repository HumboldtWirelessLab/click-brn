#ifndef CLICK_LOADBALANCER_SINK_HH
#define CLICK_LOADBALANCER_SINK_HH


CLICK_DECLS

class LoadBalancerForeignFlowHandler : public Element
{
  public:
    class ForeignFlowInfo
    {
      public:
        EtherAddress *_src_address;    // neighbor Adress
        IPAddress *_src_ip;
        IPAddress *_dst_ip;
        uint16_t _src_port;
        uint16_t _dst_port;

        ForeignFlowInfo(EtherAddress *fromAP, IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort)
        {
          _src_address = fromAP;
          _src_ip = srcIP;
          _dst_ip = dstIP;
          _src_port = srcPort;
          _dst_port = dstPort;
        }

        ~ForeignFlowInfo()
        {
          delete _src_address;
          delete _src_ip;
          delete _dst_ip;
        }
    };

    LoadBalancerForeignFlowHandler();
    ~LoadBalancerForeignFlowHandler();

/*ELEMENT*/
    const char *class_name() const  { return "LoadBalancerForeignFlowHandler"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "2/3"; }  //I: from other ap,from net O: to other AP, to net, to local redirect

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

    void *addFlow(EtherAddress *fromAP, IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort);
    void removeFlow(IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort);
    void *getFlow(IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort);

  private:

    EtherAddress _me;

    Vector<ForeignFlowInfo *> foreignFlows;

    int _debug;

};

CLICK_ENDDECLS
#endif

