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
#undef TF_NODELAY

#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/ip.h"

CLICK_DECLS

#define LWIP_MODE_UNKNOWN 0
#define LWIP_MODE_CLIENT  1
#define LWIP_MODE_SERVER  2

#define LWIP_LOWER_LAYER_PORT 0
#define LWIP_UPPER_LAYER_PORT 1

#define SRC_BUFFERSIZE 65536

//#define NEW_LWIP

class LwIP : public BRNElement
{
  public:
    class LwIPNetIf {
     public:
      uint16_t  _num;

      IPAddress _addr, _gw, _mask;

#ifdef NEW_LWIP
      ip4_addr  _lw_addr, _lw_gw, _lw_mask;
#else
      ip_addr  _lw_addr, _lw_gw, _lw_mask;
#endif

      struct netif click_netif;

      LwIP *_lwIP;     //ref to LwIP

      LwIPNetIf(uint16_t num, IPAddress addr, IPAddress gw, IPAddress mask): _num(num), _addr(addr), _gw(gw), _mask(mask)
      {
        _lw_addr.addr = _addr.addr();
        _lw_gw.addr = _gw.addr();
        _lw_mask.addr = _mask.addr();
      }

    };

    class LwIPSocket {
     public:
      LwIPNetIf *_dev;

      uint16_t  _port;

      struct tcp_pcb *_tcp_pcb;

      Vector<struct tcp_pcb *> _client_sockets;

      uint16_t  _mode;

      LwIPSocket(LwIPNetIf *dev, uint16_t port): _dev(dev), _port(port)
      {
        _mode = LWIP_MODE_SERVER;
        _tcp_pcb = tcp_new();

        if ( _tcp_pcb != NULL ) {
          tcp_arg(_tcp_pcb, (void*)this);
        } else {
          click_chatter("Couldn't get new tcp_pcb");
        }
      }

      void add_client(struct tcp_pcb *new_tcp_pcb) {
        _client_sockets.push_back(new_tcp_pcb);
      }
    };

    typedef Vector<LwIPSocket*> LwIPSocketList;
    typedef LwIPSocketList::const_iterator LwIPSocketListIter;

    class LwIPConnection {
     public:
      LwIPNetIf  *_dev;
      LwIPSocket *_local_socket;

      IPAddress _dst;
      uint16_t  _port;

#ifdef NEW_LWIP
      ip4_addr  _lw_dst;
#else
      ip_addr  _lw_dst;
#endif

      uint32_t _sent_bytes;
      uint32_t _received_bytes;

      uint32_t _flow_size;

      bool _closed;

      LwIPConnection(LwIPSocket *socket, IPAddress dst_addr, uint16_t dst_port) : _dev(NULL), _dst(dst_addr), _port(dst_port),
                                                                                   _sent_bytes(0), _received_bytes(0), _flow_size(0),
                                                                                   _closed(false)
      {
        _local_socket = socket;
        _dev = _local_socket->_dev;
        _lw_dst.addr = _dst.addr();
      }

      uint16_t bytes_left() { return _flow_size - _sent_bytes; }

      void update_stats(uint32_t send_data_count, uint32_t recv_data_count) {
        _sent_bytes += send_data_count;
        _received_bytes += recv_data_count;
      }
    };

    typedef Vector<LwIPConnection*> LwIPConnectionList;
    typedef LwIPConnectionList::const_iterator LwIPConnectionListIter;

    /*****************/
    /** M E M B E R **/
    /*****************/

    LwIP();
    ~LwIP();

/*ELEMENT*/
    const char *class_name() const  { return "LwIP"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "2/2-3"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push(int port, Packet *packet);

    bool run_task(Task *);
    void run_timer(Timer *t);

    void add_handlers();

    String xml_stats();

    LwIPNetIf *new_netif(IPAddress addr, IPAddress gw, IPAddress mask);
    LwIPSocket *new_socket(LwIPNetIf *netif, uint16_t port);
    LwIPConnection *new_connection(LwIPNetIf *netif, IPAddress dst_addr, uint16_t dst_port);

    void sent_packet(WritablePacket *packet);

    int client_add_flow(IPAddress dst_addr, uint32_t dst_port, int count);
    int client_task();
    int client_fill_buffer(LwIPSocket *sock, int count_bytes);

    IPAddress _ip_adress;
    IPAddress _gateway;
    IPAddress _netmask;

    int32_t   _server_port;

    LwIPNetIf *_interface;

    LwIPSocketList _sockets;
    LwIPConnectionList _connections;

    Timer _timer;
    uint32_t _local_tmr_counter;

    Task _task;

    struct pbuf *pbuf_in;
    Vector<WritablePacket *> packet_out_queue;
};

CLICK_ENDDECLS
#endif
