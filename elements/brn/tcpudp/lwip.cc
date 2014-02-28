#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/nameinfo.hh>
#include <click/packet.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/standard/scheduleinfo.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "lwip.hh"
#include <netinet/ip.h>

CLICK_DECLS

static Vector<void*> _netifs_list;

LwIP::LwIP()
  : _timer(this),
    _socket_id(0),
    _client(false),
    _client_send_data(0),
    _server_recv_data(0),
    _task(this)
{
  BRNElement::init();

  memset(_netifs,0,sizeof(_netifs));
  p_buf = NULL;
}

LwIP::~LwIP()
{
}

int LwIP::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CLIENT", cpkP, cpBool, &_client,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int LwIP::initialize(ErrorHandler *errh)
{
  for ( int i = 0; i < 4000; i++) buf[i] = htonl(i);

  click_brn_srandom();

  _timer.initialize(this);
  _timer.schedule_after_msec(100);

  ScheduleInfo::join_scheduler(this, &_task, errh);

  if ( !_client) {
    /* lwip: */
    BRN_DEBUG("LWIP Init");
    lwip_init();

    BRN_DEBUG("New Server Socket %p",this);
    int netif_id = new_netif(IPAddress("192.168.0.2"), IPAddress("192.168.0.1"), IPAddress("255.255.255.0"));
    int socket_id = new_socket(_netifs[netif_id], 12345);
  }

  return 0;
}

static err_t
click_lw_ip_tcp_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
  click_chatter("Sent %d Bytes", len);
  return ERR_OK;
}


void
LwIP::run_timer(Timer *t)
{
  BRN_DEBUG("Run timer.");
  _timer.reschedule_after_msec(TCP_TMR_INTERVAL);

  if ( _client ) {
    if (_sockets.size() == 0) {
      BRN_DEBUG("New client Socket %p",this);
      int netif_id = new_netif(IPAddress("192.168.0.3"), IPAddress("192.168.0.1"), IPAddress("255.255.255.0"));
      int socket_id = new_connection(_netifs[netif_id], IPAddress("192.168.0.2"), 12345);
      tcp_sent(_sockets[0]->_tcp_pcb, click_lw_ip_tcp_sent);
    } else {
      u16_t buf_size = tcp_sndbuf(_sockets[0]->_tcp_pcb);
      click_chatter("Buffersize: %d", buf_size);
      if ( _client_send_data < 625 ) {
        err_t res = tcp_write(_sockets[0]->_tcp_pcb, &buf[_client_send_data*4], 375 * 4, TCP_WRITE_FLAG_COPY);
        if ( res != ERR_OK ) {
          BRN_ERROR("tcp_write failed");
        } else {
          _client_send_data += 375;
        }
      }
    }
  }

  if ( _client ) click_chatter("client tmr");
  else click_chatter("server tmr");

  if ( !_client ) tcp_tmr();

  if ( _client ) click_chatter("client tmr end");
  else click_chatter("server tmr end");

}

bool
LwIP::run_task(Task *)
{
  click_chatter("Run Task");
  if ( p_buf == NULL ) return false;

  struct pbuf *local_pbuf = p_buf;
  p_buf = NULL;

  struct netif *global_net_if = &(((LwIP::LwIPNetIf*)_netifs_list[_netifs_list.size()-1])->click_netif); //global
  global_net_if = &_netifs[0]->click_netif;                                                              //local

  BRN_DEBUG("Input if: %p (%p)",global_net_if, global_net_if->state);
  global_net_if->input(local_pbuf, global_net_if);

  return false;
}

/**
 *
 * @param
 * @param packet
 */
void
LwIP::push( int port, Packet *packet )
{
  if ( port == LWIP_LOWER_LAYER_PORT ) {
    struct pbuf *p;

    p = pbuf_alloc(PBUF_LINK, packet->length(), PBUF_POOL);

    if (p != NULL) {
      memcpy(p->payload, packet->data(), packet->length());
      p_buf = p;
      _task.reschedule();
    }
  }

}

/*************************************************************************************************/
/********************** LWIP S T A T I C   F U N C T I O N S *************************************/
/*************************************************************************************************/

static err_t
click_lw_ip_if_link_output(struct netif *netif, struct pbuf *p)
{
  click_chatter("Link output");
  if ( (netif != NULL) && (p != NULL) ) click_chatter("Linkoutput");
  return ERR_OK;
}

static err_t
click_lw_ip_if_output(struct netif *netif, struct pbuf *p, ip_addr* ip)
{
  if ( (netif != NULL) && (p != NULL) ) click_chatter("output: %s",IPAddress(ip->addr).unparse().c_str());

  WritablePacket *p_out = WritablePacket::make(128, p->payload, p->len, 32);
  const click_ip *iph = (click_ip *)p_out->data();
  IPAddress src_addr = IPAddress(iph->ip_src);

  click_chatter("Have packet: %d State: %p Src: %s", p_out->length(), netif->state, src_addr.unparse().c_str());

  for ( int i = 0; i < _netifs_list.size(); i++ ) {
    if ( ((LwIP::LwIPNetIf*)_netifs_list[i])->_addr == src_addr ) {
      ((LwIP*)(((LwIP::LwIPNetIf*)_netifs_list[i])->click_netif.state))->output(LWIP_LOWER_LAYER_PORT).push(p_out);
      break;
    }
  }

  click_chatter("p on the way");

  return ERR_OK;
}

static err_t
click_lw_ip_if_init(struct netif *netif)
{
  click_chatter("LWIP init: %p", netif->state);

  netif->name[0] = 'i';
  netif->name[1] = 'p';

  netif->hwaddr_len = 6;
  memset(netif->hwaddr,0,netif->hwaddr_len);
  netif->hwaddr[5] = netif->num + 1;

  netif->flags |= NETIF_FLAG_BROADCAST;

  netif->output = click_lw_ip_if_output;
  netif->linkoutput = click_lw_ip_if_link_output;

  return ERR_OK;
}

err_t
click_lw_ip_tcp_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
  click_chatter("Connected");
  return ERR_OK;
}

err_t
click_lw_ip_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
  LwIP::LwIPSocket *lwip_socket = (LwIP::LwIPSocket*)arg;
  click_chatter("received");

  tcp_recved(pcb, p->len);

  return ERR_OK;
}

err_t
click_lw_ip_accept(void *arg, struct tcp_pcb *new_tpcb, err_t err)
{
  LwIP::LwIPSocket *lwip_socket = (LwIP::LwIPSocket*)arg;

  click_chatter("accept");

  lwip_socket->add_client(new_tpcb);
  tcp_accepted(lwip_socket->_listen_tcp_pcb);

  tcp_recv(new_tpcb, click_lw_ip_recv);

  return ERR_OK;
}
/***************************************************************************************************/
/********************** LWIP W R A P P E R   F U N C T I O N S *************************************/
/***************************************************************************************************/

int
LwIP::new_netif(IPAddress addr, IPAddress gw, IPAddress mask)
{
  uint16_t next_if_id = 0;

  for (;next_if_id < LWIP_MAX_INTERFACES; next_if_id++) {
    if ( _netifs[next_if_id] == NULL ) break;
  }

  if ( next_if_id == LWIP_MAX_INTERFACES ) {
    BRN_WARN("No device left");
    return -1;
  }

  LwIPNetIf *lwipnetif = new LwIPNetIf(next_if_id, addr, gw, mask);
  lwipnetif->_lwIP = this; //set to self to find object for interface //TODO: this should be done by lwip

  ip_addr lw_gw, lw_addr, lw_mask;
  lw_addr.addr = addr.addr();
  lw_gw.addr = gw.addr();
  lw_mask.addr = mask.addr();

  lwipnetif->click_netif.num = next_if_id;

  click_chatter("Netif add");
  struct netif *res = netif_add(&lwipnetif->click_netif, &lw_addr, &lw_mask, &lw_gw, (void*)this, click_lw_ip_if_init, ip_input);

  click_chatter("RES: %p (org: %p)",res,&lwipnetif->click_netif);

  netif_set_up(&lwipnetif->click_netif);
  _netifs[next_if_id] = lwipnetif;

  _netifs_list.push_back(lwipnetif);

  click_chatter("Finish");

  return next_if_id;
}

int
LwIP::new_socket(LwIPNetIf *netif, uint16_t port)
{
  LwIPSocket *new_socket = new LwIPSocket(netif, netif->_addr, port);
  new_socket->_mode = LWIP_MODE_SERVER;
  new_socket->_id = _socket_id++;

  _sockets.push_back(new_socket);

  err_t res = tcp_bind(new_socket->_tcp_pcb, &(new_socket->_lw_addr), port);
  new_socket->_listen_tcp_pcb = tcp_listen(new_socket->_tcp_pcb);

  tcp_accept(new_socket->_listen_tcp_pcb, click_lw_ip_accept);

  return(new_socket->_id);
}

int
LwIP::new_connection(LwIPNetIf *netif, IPAddress dst_addr, uint16_t dst_port)
{
  LwIPSocket *new_socket = new LwIPSocket(netif, dst_addr, dst_port);
  new_socket->_mode = LWIP_MODE_CLIENT;
  new_socket->_id = _socket_id++;

  ip_addr lw_dst_addr;
  lw_dst_addr.addr = dst_addr.addr();
  err_t res = tcp_connect( new_socket->_tcp_pcb , &lw_dst_addr, (u16_t)dst_port, click_lw_ip_tcp_connected);

  if ( res == ERR_OK ) _sockets.push_back(new_socket);

  return(new_socket->_id);
}

/****************************************************************************/
/********************** H A N D L E R   *************************************/
/****************************************************************************/

String
LwIP::xml_stats()
{
  StringAccum sa;
  Timestamp now = Timestamp::now();

  sa << "<lwip node=\"" << BRN_NODE_NAME << "\" time=\"" << now.unparse() << "\" >\n";
  sa << "</lwip>\n";
  return sa.take_string();
}

enum {
  H_LWIP_STATS
};

static String
LwIP_read_param(Element *e, void *thunk)
{
  LwIP *sf = static_cast<LwIP *>(e);

  switch ((uintptr_t) thunk) {
    case H_LWIP_STATS: {
      return sf->xml_stats();
    }
    default:
      return String();
  }
}

//static int
//LwIP_write_param(const String &/*in_s*/, Element */*e*/, void */*vparam*/, ErrorHandler */*errh*/)
//{
 //  LwIP *sf = static_cast<LwIP *>(e);
 //  String s = cp_uncomment(in_s);
 //  switch((long)vparam) {
 //  }
//  return 0;
//}


void LwIP::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", LwIP_read_param, (void *)H_LWIP_STATS);

  add_task_handlers(&_task);
}

EXPORT_ELEMENT(LwIP)
CLICK_ENDDECLS
