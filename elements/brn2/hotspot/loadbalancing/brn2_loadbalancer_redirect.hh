#ifndef CLICK_LOADBALANCER_HH
#define CLICK_LOADBALANCER_HH

#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn2/hotspot/loadbalancing/brn2_lb_rerouting.hh"

CLICK_DECLS

class LoadBalancerRedirect : public Element
{
  public:
    class FlowInfo
    {
      public:
        EtherAddress *_src_address;
        EtherAddress *_redirect_ap;
        IPAddress *_src_ip;
        IPAddress *_dst_ip;
        uint16_t _src_port;
        uint16_t _dst_port;

        FlowInfo(EtherAddress *srcEtherAddress,EtherAddress *redirectAP, IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort)
        {
          _src_address = srcEtherAddress;
          _redirect_ap = redirectAP;
          _src_ip = srcIP;
          _dst_ip = dstIP;
          _src_port = srcPort;
          _dst_port = dstPort;
        }

        ~FlowInfo()
        {
          delete _src_address;
          delete _redirect_ap;
          delete _src_ip;
          delete _dst_ip;
        }
    };

    LoadBalancerRedirect();
    ~LoadBalancerRedirect();

/*ELEMENT*/
    const char *class_name() const  { return "LoadBalancerRedirect"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "3/3"; }  //Input: Local Client, other AP,from local foreign   Output: Local Client, Local Internet, Redirect to other AP

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

    void addFlow(EtherAddress *srcEtherAddress,EtherAddress *redirectAP, IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort);

    void removeFlow(EtherAddress *srcEtherAddress, IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort);

    EtherAddress *getNodeForFlow(IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort);

    EtherAddress* getBestNodeForFlow(EtherAddress *srcEtherAddress, IPAddress *srcIP, IPAddress *dstIP,uint16_t srcPort, uint16_t dstPort);

    EtherAddress *getSrcForFlow(IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort);

    void nodeDetection();

  private:

    EtherAddress _me;

    BRN2LinkStat *_linkstat;

    Vector<EtherAddress> _neighbors;

    Vector<FlowInfo *> redirectFlows;

    LoadbalancingRerouting *_rerouting;

    int _debug;

};

CLICK_ENDDECLS
#endif
