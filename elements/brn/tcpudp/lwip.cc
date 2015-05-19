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

static Vector<void*> _static_netifs_list;
static int called_lwip_init = 0;
static uint32_t _global_tmr_counter = 0;

LwIP::LwIP()
  : _server_port(-1),
    _dst_address(),
    _dst_port(-1),
    _socket_id(0),
    _client_send_data(0),
    _server_recv_data(0),
    _timer(this),
    _local_tmr_counter(_global_tmr_counter),
    _task(this)
{
  BRNElement::init();

  _interface = NULL;
  pbuf_in = NULL;
  packet_out_queue.clear();
}

LwIP::~LwIP()
{
}

int LwIP::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "IP", cpkP+cpkM, cpIPAddress, &_ip_adress,
      "GATEWAY", cpkP+cpkM, cpIPAddress, &_gateway,
      "NETMASK", cpkP+cpkM, cpIPAddress, &_netmask,
      "DSTIP", cpkP, cpIPAddress, &_dst_address,
      "DSTPORT", cpkP, cpInteger, &_dst_port,
      "SERVERPORT", cpkP, cpInteger, &_server_port,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  _is_server = (_server_port != -1);

  if ( called_lwip_init == 0 ) {
    BRN_DEBUG("LWIP Init");
    lwip_init();
    called_lwip_init = 1;
  }

  return 0;
}

int LwIP::initialize(ErrorHandler *errh)
{
  for ( int i = 0; i < SRC_BUFFERSIZE; i++) buf[i] = htonl(i);

  click_brn_srandom();

  _timer.initialize(this);
  _timer.schedule_after_msec(TCP_TMR_INTERVAL);

  ScheduleInfo::join_scheduler(this, &_task, errh);

  BRN_DEBUG("New Device %p",this);
  _interface = new_netif(_ip_adress, _gateway, _netmask);
  _static_netifs_list.push_back(_interface);

  if (_is_server) {
    /* lwip: */
    BRN_DEBUG("New Server Socket %p",this);
    LwIPSocket *new_sock = new_socket(_interface, _server_port);
    _sockets.push_back(new_sock);

    if ( new_sock == NULL ) {
      BRN_ERROR("New socket failed");
    } else {
      BRN_DEBUG("Server-Socket: %s:%d",_interface->_addr.unparse().c_str(), new_sock->_port);
    }
  }

  return 0;
}

void
LwIP::run_timer(Timer */*t*/)
{
  _timer.schedule_after_msec(TCP_TMR_INTERVAL/10);

  //only one will call tcp_tmr
  if (_global_tmr_counter == _local_tmr_counter) {
    BRN_DEBUG("Call tmr");
    tcp_tmr();                                      //TODO: what about: ip_reass_tmr() & sys_check_timeouts()
    _global_tmr_counter++;
  }

  _local_tmr_counter = _global_tmr_counter;

  if (!_is_server) client_task();
}

bool
LwIP::run_task(Task *)
{
  BRN_DEBUG("Run Task: %s",Timestamp::now().unparse().c_str());

  if (packet_out_queue.size() != 0) {
    for (int i = 0; i < packet_out_queue.size(); i++)
      output(LWIP_LOWER_LAYER_PORT).push(packet_out_queue[i]);

    packet_out_queue.clear();
  }

  if ( pbuf_in != NULL ) {
    struct pbuf *local_pbuf = pbuf_in;
    pbuf_in = NULL;

    BRN_DEBUG("Netifs: %d",_static_netifs_list.size());
    BRN_DEBUG("Input if: %p (%p)", &(_interface->click_netif), _interface->click_netif.state);

    _interface->click_netif.input(local_pbuf, &(_interface->click_netif));
  }

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
      pbuf_in = p;
      click_chatter("Push: %s",Timestamp::now().unparse().c_str());
      _task.reschedule();
    }
  }
}

void
LwIP::sent_packet(WritablePacket *packet)
{
  BRN_DEBUG("sent packet");
  //assert(packet_out_queue.size() == 0);

  if (packet_out_queue.size() > 0) {
    output(LWIP_LOWER_LAYER_PORT).push(packet_out_queue[0]);
    packet_out_queue.clear();
  }

  packet_out_queue.push_back(packet);

  _task.reschedule();
}

int
LwIP::client_fill_buffer(LwIPSocket *sock, int count_bytes)
{
  int buf_size = (int)tcp_sndbuf(sock->_tcp_pcb);
  int bytes_left = MIN(count_bytes,buf_size);

  bytes_left -= (bytes_left%4);
  int bytes_written = bytes_left;

  BRN_DEBUG("Buffersize: %d Client_send_data: %d", buf_size, bytes_left);

  int bytes_per_write = 100;
  bytes_per_write = MIN(bytes_per_write,bytes_left);

  while (bytes_left > 0) {
    err_t res = tcp_write(sock->_tcp_pcb, buf, bytes_per_write, TCP_WRITE_FLAG_COPY);
    if ( res != ERR_OK ) {
      BRN_ERROR("tcp_write failed");
    } else {
      bytes_left -= bytes_per_write;
      bytes_per_write = MIN(bytes_per_write,bytes_left);
    }
  }

  return bytes_written;
}

int
LwIP::client_task()
{
  if (_is_server ) return 0;

  if (_sockets.size() == 0) {

    BRN_DEBUG("New client Socket %p (%s -> %s:%d",this,_interface->_addr.unparse().c_str(),
                                                  _dst_address.unparse().c_str(), _dst_port);
    LwIPSocket *new_sock = new_connection(_interface, _dst_address, _dst_port);

    BRN_DEBUG("Client Socket-ID: %p", new_sock);
    if ( new_sock == NULL ) {
      BRN_ERROR("New connection failed");
    }
    _client_send_data = 100000;
  }

  BRN_DEBUG("Clientdata left: %d", _client_send_data);

  if ( _client_send_data > 0 ) {
    _client_send_data -= client_fill_buffer(_sockets[0], _client_send_data);
  } else if (_client_send_data == 0) {
    BRN_DEBUG("Close TCP-Connection");
    err_t res = tcp_close(_sockets[0]->_tcp_pcb);
    if ( res != ERR_OK ) {
      BRN_ERROR("tcp_close failed");
    } else {
      BRN_ERROR("tcp_close ok");
    }
    _client_send_data--;
  }

  return 0;
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
click_lw_ip_if_output(struct netif *netif, struct pbuf *p, ip_addr* ip) //TODO: ip == dst ?
{
  IPAddress param_addr = IPAddress(ip->addr);

  if ( (netif != NULL) && (p != NULL) ) click_chatter("if output: Dst: %s",param_addr.unparse().c_str());
  else click_chatter("if output with NULL-params");

  WritablePacket *packet_out = WritablePacket::make(128, p->payload, p->len, 32);

  const click_ip *iph = (click_ip *)packet_out->data();
  IPAddress src_addr = IPAddress(iph->ip_src);
  IPAddress dst_addr = IPAddress(iph->ip_dst);

  click_chatter("Have packet: %d State: %p Src: %s Dst: %s Param: %s", packet_out->length(), netif->state,
                                                                       src_addr.unparse().c_str(),
                                                                       dst_addr.unparse().c_str(),
                                                                       param_addr.unparse().c_str());

  for ( int i = 0; i < _static_netifs_list.size(); i++ ) {
    if ( ((LwIP::LwIPNetIf*)_static_netifs_list[i])->_addr == src_addr ) {
      //((LwIP*)(((LwIP::LwIPNetIf*)_static_netifs_list[i])->click_netif.state))->output(LWIP_LOWER_LAYER_PORT).push(packet_out);
      ((LwIP*)(((LwIP::LwIPNetIf*)_static_netifs_list[i])->click_netif.state))->sent_packet(packet_out);
      break;
    }
  }

  click_chatter("packet is out %s -> %s", src_addr.unparse().c_str(), dst_addr.unparse().c_str());

  return ERR_OK;
}

static err_t
click_lw_ip_if_init(struct netif *netif)
{
  click_chatter("LWIP Interface init: %p", netif->state);

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
click_lw_ip_tcp_connected(void */*arg*/, struct tcp_pcb */*tpcb*/, err_t /*err*/)
{
  click_chatter("Connected");
  return ERR_OK;
}

static err_t
click_lw_ip_tcp_sent(void *arg, struct tcp_pcb */*tpcb*/, u16_t len)
{
  click_chatter("Callback: Sent %d Bytes", len);

  LwIP::LwIPSocket *lwip_socket = (LwIP::LwIPSocket*)arg;
  lwip_socket->_dev->_lwIP->client_task();

  return ERR_OK;
}


err_t
click_lw_ip_tcp_recv(void */*arg*/, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
  if (err == ERR_OK ) {
    if (p != NULL) {
      click_chatter("Callback: received: %d bytes", p->tot_len);
      tcp_recved(pcb, p->tot_len);
      pbuf_free(p);
    } else {
      click_chatter("Callback: received: p == NULL");
      tcp_arg(pcb, NULL);
      tcp_sent(pcb, NULL);
      tcp_recv(pcb, NULL);
      tcp_close(pcb);
    }
  }

  return ERR_OK;
}

err_t
click_lw_ip_tcp_accept(void *arg, struct tcp_pcb *new_tpcb, err_t /*err*/)
{
  LwIP::LwIPSocket *lwip_socket = (LwIP::LwIPSocket*)arg;

  click_chatter("accept");

  lwip_socket->add_client(new_tpcb);
  tcp_accepted(lwip_socket->_tcp_pcb);

  tcp_recv(new_tpcb, click_lw_ip_tcp_recv);
  tcp_sent(new_tpcb, click_lw_ip_tcp_sent);

  return ERR_OK;
}

/***************************************************************************************************/
/********************** LWIP W R A P P E R   F U N C T I O N S *************************************/
/***************************************************************************************************/

LwIP::LwIPNetIf *
LwIP::new_netif(IPAddress addr, IPAddress gw, IPAddress mask)
{
  _netif_id = _static_netifs_list.size();

  LwIPNetIf *lwipnetif = new LwIPNetIf(_netif_id, addr, gw, mask);
  lwipnetif->_lwIP = this; //set to self to find object for interface //TODO: this should be done by lwip


  BRN_DEBUG("Netif add");
  struct netif *res = netif_add(&lwipnetif->click_netif, &(lwipnetif->_lw_addr), &(lwipnetif->_lw_mask), &(lwipnetif->_lw_gw),
                                (void*)this, click_lw_ip_if_init, ip_input);

  BRN_DEBUG("RES: %p (org: %p)",res,&lwipnetif->click_netif);

  netif_set_up(&(lwipnetif->click_netif));

  BRN_DEBUG("Finish: lwid: %d static_id: %d", lwipnetif->click_netif.num, _netif_id);

  return lwipnetif;
}

LwIP::LwIPSocket *
LwIP::new_socket(LwIPNetIf *netif, uint16_t port)
{
  LwIPSocket *new_sock = new LwIPSocket(netif, port);
  new_sock->_mode = (port==0)?LWIP_MODE_CLIENT:LWIP_MODE_SERVER;

  err_t res = tcp_bind(new_sock->_tcp_pcb, &(netif->_lw_addr), port);

  if ( res != ERR_OK ) {
    BRN_ERROR("Bind failed");
    delete new_sock;
    return NULL;
  }

  if ( port != 0 ) {
    struct tcp_pcb *listen_tcp_pcb = tcp_listen(new_sock->_tcp_pcb);
    tcp_accept(new_sock->_tcp_pcb, click_lw_ip_tcp_accept);

    BRN_DEBUG("Org pcb: %p new pcb: %p",new_sock->_tcp_pcb, listen_tcp_pcb);
  }

  tcp_recv(new_sock->_tcp_pcb, click_lw_ip_tcp_recv);     //set "sent function"
  tcp_sent(new_sock->_tcp_pcb, click_lw_ip_tcp_sent);     //set "recv function"

  return new_sock;
}

LwIP::LwIPSocket *
LwIP::new_connection(LwIPNetIf *netif, IPAddress dst_addr, uint16_t dst_port)
{
  LwIPSocket *new_sock = new_socket(netif,0);

  ip_addr lw_dst_addr;
  lw_dst_addr.addr = dst_addr.addr();

  BRN_DEBUG("Start new tcp-connection %s -> %s", netif->_addr.unparse().c_str(), dst_addr.unparse().c_str());
  err_t res = tcp_connect( new_sock->_tcp_pcb , &lw_dst_addr, (u16_t)dst_port, click_lw_ip_tcp_connected);

  BRN_DEBUG("pcb: %s:%d -> %s:%d", IPAddress(new_sock->_tcp_pcb->local_ip.addr).unparse().c_str(),
                                   new_sock->_tcp_pcb->local_port,
                                   IPAddress(new_sock->_tcp_pcb->remote_ip.addr).unparse().c_str(),
                                   new_sock->_tcp_pcb->remote_port);

  BRN_DEBUG("finished setup Connection %s -> %s", netif->_addr.unparse().c_str(), dst_addr.unparse().c_str());

  if ( new_sock != NULL ) {
    _sockets.push_back(new_sock);
  } else {
    BRN_ERROR("Connection err");
  }

  return new_sock;
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
