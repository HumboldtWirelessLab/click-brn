#ifndef CLICK_LWIP_HH
#define CLICK_LWIP_HH
#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/task.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnelement.hh"

#undef IP_RF
#undef IP_DF
#undef IP_MF
#undef IP_OFFMASK

#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"

struct n;
CLICK_DECLS

#define LWIP_MODE_UNKNOWN 0
#define LWIP_MODE_CLIENT  1
#define LWIP_MODE_SERVER  2

#define LWIP_MAX_INTERFACES 16

#define LWIP_LOWER_LAYER_PORT 0
#define LWIP_UPPER_LAYER_PORT 1
#define LWIP_MAX_INTERFACES 16


class LwIP : public BRNElement
{
  public:
    class LwIPNetIf {
     public:
      uint16_t  _num;
      IPAddress _addr;
      IPAddress _gw;
      IPAddress _mask;

      struct netif click_netif;

      LwIP *_lwIP;

      LwIPNetIf(uint16_t num, IPAddress addr, IPAddress gw, IPAddress mask) : _num(num), _addr(addr), _gw(gw), _mask(mask)
      {
      }

    };

    class LwIPSocket {
     public:
      LwIPNetIf *_dev;
      IPAddress _addr;
      struct ip_addr _lw_addr;
      uint16_t  _port;

      struct tcp_pcb *_tcp_pcb;
      struct tcp_pcb *_listen_tcp_pcb;

      Vector<struct tcp_pcb *> _client_sockets;

      uint16_t  _mode;

      uint32_t _id;

      LwIPSocket(LwIPNetIf *dev, IPAddress addr, uint16_t port): _dev(dev), _addr(addr), _port(port)
      {
        _mode = LWIP_MODE_SERVER;
        _lw_addr.addr = _addr.addr();
        _tcp_pcb = tcp_new();
        tcp_arg(_tcp_pcb, (void*)this);
      }

      void add_client(struct tcp_pcb *new_tcp_pcb) {
        _client_sockets.push_back(new_tcp_pcb);
      }
    };

    typedef Vector<LwIPSocket*> LwIPSocketList;
    typedef LwIPSocketList::const_iterator LwIPSocketListIter;

    Timer _timer;

    /*****************/
    /** M E M B E R **/
    /*****************/

    LwIP();
    ~LwIP();

/*ELEMENT*/
    const char *class_name() const  { return "LwIP"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "2/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push(int port, Packet *packet);
    bool run_task(Task *);

    void add_handlers();

    void run_timer(Timer *t);

    String xml_stats();

    int new_netif(IPAddress addr, IPAddress gw, IPAddress mask);
    int new_socket(LwIPNetIf *netif, uint16_t port);
    int new_connection(LwIPNetIf *netif, IPAddress dst_addr, uint16_t dst_port);

    LwIPNetIf *_netifs[LWIP_MAX_INTERFACES];
    LwIPSocketList _sockets;

    uint32_t _socket_id;

    uint32_t buf[4000];

    bool _client;
    int _client_send_data;

    int _server_recv_data;

    Task _task;
    struct pbuf *p_buf;
};

CLICK_ENDDECLS
#endif
