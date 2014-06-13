#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/nameinfo.hh>
#include <click/packet.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brn2_simpleflow.hh"

#ifdef SF_TX_ABORT
#if CLICK_NS
#include <click/router.hh>
#include <click/simclick.h>
#include "elements/brn/routing/identity/txcontrol.h"
#endif
#endif

CLICK_DECLS

#ifdef SF_TX_ABORT
#if CLICK_NS
void
BRN2SimpleFlow::abort_transmission(EtherAddress &dst)
{
  struct tx_control_header txch;

  if ( (click_random() % 10) > 5 ) return;

  txch.operation = TX_ABORT;
  txch.flags = 0;
  memcpy(txch.dst_ea, dst.data(), 6);

  BRN_ERROR("Abort tx: %s", dst.unparse().c_str());
  simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_TX_CONTROL, &txch);

  if ( txch.flags != 0 ) BRN_ERROR("TXCtrl-Error");
}
#endif
#endif

BRN2SimpleFlow::BRN2SimpleFlow()
  : _timer(this),
    _clear_packet(true),
    _headersize(-1),
    _headroom(128),
    _flow_id(0),
    _simpleflow_element_id(0),
    _routing_peek(NULL),
    _link_table(NULL),
    _flow_start_rand(0)
{
  BRNElement::init();
}

BRN2SimpleFlow::~BRN2SimpleFlow()
{
  reset();
}

int BRN2SimpleFlow::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _init_flow = "";
  _extra_data = "";

  if (cp_va_kparse(conf, this, errh,
      "FLOW", cpkP, cpString, &_init_flow,
      "ELEMENTID", cpkP, cpInteger, &_simpleflow_element_id,
      "EXTRADATA", cpkP, cpString, &_extra_data,
      "CLEARPACKET", cpkP, cpBool, &_clear_packet,
      "HEADERSIZE", cpkP, cpInteger, &_headersize,
      "HEADROOM", cpkP, cpInteger, &_headroom,
      "ROUTINGPEEK", cpkP, cpElement, &_routing_peek,
      "LT", cpkP, cpElement, &_link_table,
      "FLOWSTARTRANDOM", cpkP, cpInteger, &_flow_start_rand,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  _headroom += sizeof(struct click_brn);  // space for header

  return 0;
}

static bool routing_peek_func(void *e, Packet *p, EtherAddress *src, EtherAddress *dst, int brn_port)
{
  return ((BRN2SimpleFlow *)e)->handle_routing_peek(p, src, dst, brn_port);
}

int BRN2SimpleFlow::initialize(ErrorHandler *)
{
  click_brn_srandom();
  //don't move this to configure, since BRNNodeIdenty is not configured
  //completely while configure this element, so set_active can cause
  //seg, fault, while calling BRN_DEBUG in set_active
  _timer.initialize(this);

  if (_routing_peek) {
    if (-1 == _routing_peek->add_routing_peek(routing_peek_func, (void*)this, BRN_PORT_FLOW ))
      BRN_ERROR("Failed adding routing_peek_func()");
  }

  if ( _init_flow != "" ) add_flow(_init_flow);

  return 0;
}

void
BRN2SimpleFlow::run_timer(Timer *t)
{
  BRN_DEBUG("Run timer.");

  if ( t == NULL ) BRN_ERROR("Timer is NULL");

  Timestamp now = Timestamp::now();
  int active_flows = 0;

  for (BRN2SimpleFlow::FMIter fm = _tx_flowMap.begin(); fm.live(); ++fm) {
    BRN2SimpleFlow::Flow *fl = fm.value();

    BRN_DEBUG("Flow: Start: %s End: %s Active: %d",fl->_start_time.unparse().c_str(), fl->_end_time.unparse().c_str(), fl->_active?1:0);

    BRN_DEBUG("Flow: Time to start: %d", (fl->_start_time-now).msecval());
    BRN_DEBUG("Flow: Time to end: %d", (fl->_end_time-now).msecval());

    if ( (fl->_end_time-now).msecval() <= 0 ) {
      BRN_DEBUG("deactive flow");
      fl->_active = false;                      //TODO: check earlier
      continue;                                 //finished flow. so take next!!
    }

    if ( !fl->_active ) {
      if ( (fl->_start_time - now).msecval() <= 0 ) fl->_active = true; //start is in the past: flow is active
    } else {
      if ( fl->_interval == 0 ) continue;                    //interval = 0 -> packet send on feedback
    }

    active_flows++;

    send_packets_for_flow(fl);
  }

  if ( active_flows != 0 ) schedule_next();

}

void
BRN2SimpleFlow::set_active(Flow *txFlow, bool active)
{
  BRN_DEBUG("set_active");

  if (active && (!is_active(txFlow))) {
    BRN_DEBUG("flow actived");
    txFlow->_active = true;
    txFlow->_start_time = Timestamp::now();
    txFlow->_next_time = Timestamp::now();

    send_packets_for_flow(txFlow);

    if ( txFlow->_interval != 0 ) schedule_next();

  } else if ((!active) && is_active(txFlow)) {
    BRN_DEBUG("flow deactived");
    _timer.clear();
    txFlow->_active = active;
  }
}

bool
BRN2SimpleFlow::is_active(Flow *txFlow)
{
  if ( txFlow ) {
    Timestamp now = Timestamp::now();
    if ( txFlow->_active ) {
      BRN_DEBUG("%s %s %d %d",now.unparse().c_str(), txFlow->_start_time.unparse().c_str(),(now - txFlow->_start_time).msecval(), txFlow->_duration);
      if ( (now <= txFlow->_end_time) || (txFlow->_duration == 0)/*endless*/ ) {
        return true;
      } else {
        txFlow->_active = false;
      }
    }
  }

  return false;
}

void
BRN2SimpleFlow::send_packets_for_flow(Flow *fl)
{
  if ( is_active(fl) ) {
    BRN_DEBUG("Flow (active): Src: %s Dst: %s ID: %d interval: %d burst: %d",
               fl->_src.unparse().c_str(), fl->_dst.unparse().c_str(), fl->_id, fl->_interval, fl->_burst);

    for( uint32_t i = 0; i < fl->_burst; i++ ) {
      BRN_DEBUG("Send Next");
      Packet *packet_out = nextPacketforFlow(fl);
      output(0).push(packet_out);
    }

    fl->_next_time = fl->_next_time + Timestamp::make_msec(fl->_interval/1000, fl->_interval%1000);
  }
}

void
BRN2SimpleFlow::schedule_next()
{
  int shortest_next_time = -1;
  Timestamp now = Timestamp::now();

  int next_time;

  BRN_DEBUG("NO. Flows: %d", _tx_flowMap.size());
  for (BRN2SimpleFlow::FMIter fm = _tx_flowMap.begin(); fm.live(); ++fm) {
    BRN2SimpleFlow::Flow *fl = fm.value();

    next_time = (fl->_start_time - now).msecval();

    BRN_DEBUG("Next start schedule: %d", next_time);
    if ( next_time > 0 ) {        //next action for flow is to start it
     BRN_DEBUG("Start for flow: %s (in %d ms)", fl->_start_time.unparse().c_str(), next_time);
    } else {
      BRN_DEBUG("Start is done. Active: %d",fl->_active?1:0);
      if ( ! fl->_active ) continue; //flow is not active
      if (fl->_interval == 0) {      //interval == 0 -> packet send by feedback reuse
        continue;
      }

      next_time = (fl->_next_time - now).msecval();
      BRN_DEBUG("Next Time: %d", next_time);
    }

    if ( (shortest_next_time == -1) || ( next_time < shortest_next_time ) ) {
      shortest_next_time = next_time;
    }
  }

  if ( shortest_next_time != -1 ) {
    BRN_DEBUG("Next schedule in %d msecs",shortest_next_time);
    if (_timer.scheduled() ) {
      BRN_WARN("Timer is already scheduled");
      BRN_DEBUG("Timer is scheduled at %s",_timer.expiry().unparse().c_str());
      _timer.unschedule();
      _timer.schedule_after_msec(shortest_next_time);
      //_timer.reschedule_after_msec(shortest_next_time);
    } else {
      _timer.schedule_after_msec(shortest_next_time);
    }

    BRN_DEBUG("Timer is new scheduled at %s",_timer.expiry().unparse().c_str());
  }
}

void
BRN2SimpleFlow::add_flow( EtherAddress src, EtherAddress dst,
                          uint32_t size, uint32_t mode,
                          uint32_t interval, uint32_t burst,
                          uint32_t duration, bool active, uint32_t start_delay )
{
  Flow *txFlow = new Flow(src, dst, _flow_id, (FlowType)mode, size, interval, burst, duration);

  if ( _flow_start_rand != 0 ) start_delay += click_random() % _flow_start_rand;

  txFlow->_start_time += Timestamp::make_msec(start_delay/1000, start_delay%1000);
  txFlow->_next_time = txFlow->_start_time;
  txFlow->_end_time += Timestamp::make_msec(start_delay/1000, start_delay%1000);
  txFlow->_extra_data = _extra_data;

  _tx_flowMap.insert(FlowID(src,_flow_id), txFlow);

  _flow_id++;

  if ( start_delay == 0 ) set_active(txFlow, active);
  else schedule_next();
}

/**
 *
 * @param
 * @param packet
 */
void
BRN2SimpleFlow::push( int port, Packet *packet )
{
  struct flowPacketHeader *header = (struct flowPacketHeader *)packet->data();

  uint32_t flow_id = ntohl(header->flowID);
  uint32_t packet_id = ntohl(header->packetID);
  EtherAddress src_ea = EtherAddress(header->src);
  FlowID fid = FlowID(src_ea, flow_id);

  if ( port == 1 ) {
    if ( (header->flags & SIMPLEFLOW_FLAGS_IS_REPLY) != 0 ) packet->kill();
    else handle_reuse(packet);
    return;
  }

#ifdef SF_TX_ABORT
#if CLICK_NS
  EtherAddress dst_ea = EtherAddress(header->dst);
  abort_transmission(dst_ea);
#endif
#endif

  Timestamp send_time;
  send_time.assign(ntohl(header->tv_sec), ntohl(header->tv_usec));

  uint16_t checksum;

  /*Handle Reply*/
  if ( (header->flags & SIMPLEFLOW_FLAGS_IS_REPLY) != 0 ) {
    Flow *f_tx = _tx_flowMap.find(fid);
    if ( f_tx ) {
      BRN_DEBUG("Got reply");

      f_tx->add_rx_stats((uint32_t)(Timestamp::now() - send_time).msecval(),
                         (uint32_t)(SIMPLEFLOW_MAXHOPCOUNT - BRNPacketAnno::ttl_anno(packet)));

      packet->kill();
      return;
    }
  }

  /*Handle Packet*/
  Flow *f = _rx_flowMap.find(fid);

  if ( f == NULL ) {  //TODO: shorten this
    _rx_flowMap.insert(fid, new Flow(src_ea, EtherAddress(header->dst), ntohl(header->flowID),
                       (flowType)header->mode, ntohs(header->size), ntohs(header->interval), ntohs(header->burst), 10000) );
    f = _rx_flowMap.find(fid);
  }

  f->add_rx_stats((uint32_t)(Timestamp::now() - send_time).msecval(),
                  (uint32_t)(SIMPLEFLOW_MAXHOPCOUNT - BRNPacketAnno::ttl_anno(packet)));

#if HAVE_FAST_CHECKSUM
  checksum = ip_fast_csum((unsigned char *)packet->data() + (2 * sizeof(uint16_t)), (packet->length() - (2 * sizeof(uint16_t))) >> 2 );
#else
  checksum = click_in_cksum((unsigned char *)packet->data() + (2 * sizeof(uint16_t)), (packet->length() - (2 * sizeof(uint16_t))));
#endif
  BRN_INFO("Insum: %d",checksum);

  if ( checksum != ntohs(header->crc) ) f->_rxCrcErrors++;
  if ( packet_id < f->_max_packet_id ) f->_rx_out_of_order++;
  else f->_max_packet_id = packet_id;

  if ( header->extra_data_size != 0 ) {
    f->_extra_data = String((uint8_t*)&header[1], header->extra_data_size);
  }

  if ( (header->flags & SIMPLEFLOW_FLAGS_INCL_ROUTE) &&
       (packet->length() >= (MINIMUM_FLOW_PACKET_SIZE + header->extra_data_size + sizeof(struct flowPacketRoute))) ) {

    struct flowPacketRoute *rh = (struct flowPacketRoute *)&((uint8_t*)&header[1])[header->extra_data_size];
    uint16_t incl_hops = ntohs(rh->incl_hops);

    f->route.clear();
    f->metric.clear();
    f->metric_sum = ntohl(rh->metric_sum);
    f->route_hops = ntohs(rh->hops);

    BRN_DEBUG("Route:");
    struct flowPacketHop *pfh = (struct flowPacketHop *)&rh[1];
    for ( uint16_t i = 0; i <= incl_hops; i++) {
      EtherAddress ea = EtherAddress(pfh[i].ea);
      f->route.push_back(ea);
      f->metric.push_back(ntohs(pfh[i].metric));

      BRN_DEBUG("\tHop %d: %s (%d)",i,ea.unparse().c_str(),ntohs(pfh[i].metric));
    }

    const click_ether *ether = (const click_ether *)packet->ether_header();
    //EtherAddress psrc = EtherAddress(ether->ether_shost);
    EtherAddress pdst = EtherAddress(ether->ether_dhost);
    if ( ! (( pdst == (EtherAddress(pfh[incl_hops].ea))) || pdst.is_broadcast())) {
      f->route.push_back(pdst);
      f->route_hops++;
      if ( _link_table != NULL ) {
        f->metric.push_back(_link_table->get_link_metric(EtherAddress(pfh[incl_hops-1].ea), pdst));
        f->metric_sum += _link_table->get_link_metric(EtherAddress(pfh[incl_hops-1].ea), pdst);
        BRN_DEBUG("\tHop %d: %s (%d)", incl_hops, pdst.unparse().c_str(), _link_table->get_link_metric(EtherAddress(pfh[incl_hops-1].ea), pdst));
      } else {
        f->metric.push_back(0);
        BRN_DEBUG("\tHop %d: %s (0)", incl_hops, pdst.unparse().c_str());
      }
    }
  }

  if ( f->_type == TYPE_FULL_ACK ) {
    header->flags |= SIMPLEFLOW_FLAGS_IS_REPLY;
#if HAVE_FAST_CHECKSUM
    checksum = ip_fast_csum((unsigned char *)packet->data() + (2 * sizeof(uint16_t)),
                             (packet->length() - (2 * sizeof(uint16_t))) >> 2 );
#else
    checksum = click_in_cksum((unsigned char *)packet->data() + (2 * sizeof(uint16_t)),
                               (packet->length() - (2 * sizeof(uint16_t))));
#endif
    BRN_INFO("Reply outsum: %d",checksum);

    header->crc = htons(checksum);

    BRN_DEBUG("Reply: %s to %s",f->_dst.unparse().c_str(), f->_src.unparse().c_str());

    BRNPacketAnno::set_ether_anno(packet, f->_dst, f->_src, ETHERTYPE_BRN );
    WritablePacket *packet_out = BRNProtocol::add_brn_header(packet, BRN_PORT_FLOW, BRN_PORT_FLOW, SIMPLEFLOW_MAXHOPCOUNT,
                                                             DEFAULT_TOS);

    output(0).push(packet_out);

    return;
  }

  if ( f->_type == TYPE_SMALL_ACK ) {
    packet->take(packet->length() - sizeof(struct flowPacketHeader));
    header->flags |= SIMPLEFLOW_FLAGS_IS_REPLY;
#if HAVE_FAST_CHECKSUM
    checksum = ip_fast_csum((unsigned char *)packet->data() + (2 * sizeof(uint16_t)),
                             (packet->length() - (2 * sizeof(uint16_t))) >> 2 );
#else
    checksum = click_in_cksum((unsigned char *)packet->data() + (2 * sizeof(uint16_t)),
                               (packet->length() - (2 * sizeof(uint16_t))));
#endif
    BRN_INFO("Reply outsum: %d",checksum);

    header->crc = htons(checksum);

    BRNPacketAnno::set_ether_anno(packet, f->_dst, f->_src, ETHERTYPE_BRN );
    WritablePacket *packet_out = BRNProtocol::add_brn_header(packet, BRN_PORT_FLOW, BRN_PORT_FLOW,
                                                             SIMPLEFLOW_MAXHOPCOUNT, DEFAULT_TOS);

    output(0).push(packet_out);

    return;
  }

  packet->kill();
}

WritablePacket*
BRN2SimpleFlow::nextPacketforFlow(Flow *f)
{
  WritablePacket *p;
  WritablePacket *p_brn;
  uint32_t size = f->_size;
  uint16_t checksum;

  if ( size < MINIMUM_FLOW_PACKET_SIZE )  //TODO: warn that we extend the packet
    size = MINIMUM_FLOW_PACKET_SIZE;

  if ( f->_buffered_p != NULL ) {
    p = f->_buffered_p->uniqueify();
    f->_buffered_p = NULL;
  } else {
    p = WritablePacket::make(_headroom ,NULL /* *data*/, size - sizeof(struct click_brn), 32);
    if ( _clear_packet ) memset(p->data(), 0, size - sizeof(struct click_brn));
  }

  struct flowPacketHeader *header = (struct flowPacketHeader *)p->data();

  memcpy(header->src, f->_src.data(), 6);
  memcpy(header->dst, f->_dst.data(), 6);

  BRN_DEBUG("Mode: %d",(int)f->_type);

  header->flowID = htonl(f->_id);
  header->size = htons(f->_size);

  header->interval = htons(f->_interval);
  header->burst = htons(f->_burst);

  header->mode = f->_type;
  header->flags = 0;

  header->simpleflow_id = _simpleflow_element_id;

  header->extra_data_size = _extra_data.length();

  memcpy((uint8_t*)&header[1], _extra_data.data(), _extra_data.length());

  if ( size >= (MINIMUM_FLOW_PACKET_SIZE + _extra_data.length() + sizeof(struct flowPacketRoute))) {
    BRN_DEBUG("Packet is big. Add route");
    header->flags |= SIMPLEFLOW_FLAGS_INCL_ROUTE;

    struct flowPacketRoute *rh = (struct flowPacketRoute *)&(((uint8_t *)&header[1])[_extra_data.length()]);
    rh->hops = 0;
    rh->metric_sum = 0;
    rh->incl_hops = 0;

    BRN_DEBUG("Header: %d",(uint32_t)header->flags);

    if ( size >= (MINIMUM_FLOW_PACKET_SIZE + _extra_data.length() + sizeof(struct flowPacketRoute) + sizeof(struct flowPacketHop))) {
      struct flowPacketHop *pfh = (struct flowPacketHop *)&rh[1];
      pfh[0].metric = 0;
      memcpy(pfh[0].ea, f->_src.data(),6);
    }
  }

  Timestamp ts = Timestamp::now();

  p->set_timestamp_anno(ts);

  header->tv_sec = htonl(ts.sec());
  header->tv_usec = htonl(ts.nsec());

  header->packetID = htonl(f->_txPackets);

  if ( f->_txPackets == 0 ) {
    BRNPacketAnno::set_flow_ctrl_flags_anno(p, FLOW_CTRL_FLOW_START);
  } else if ( ! f->_active ) { //stop the flow
    BRNPacketAnno::set_flow_ctrl_flags_anno(p, FLOW_CTRL_FLOW_END);
  }

  f->_txPackets++;

#if HAVE_FAST_CHECKSUM
  checksum = ip_fast_csum((unsigned char *)p->data() + (2 * sizeof(uint16_t)), (p->length() - (2 * sizeof(uint16_t))) >> 2 );
#else
  checksum = click_in_cksum((unsigned char *)p->data() + (2 * sizeof(uint16_t)), (p->length() - (2 * sizeof(uint16_t))));
#endif
  BRN_INFO("Outsum: %d",checksum);

  header->crc = htons(checksum);

  BRNPacketAnno::set_ether_anno(p, f->_src, f->_dst, ETHERTYPE_BRN );
  p_brn = BRNProtocol::add_brn_header(p, BRN_PORT_FLOW, BRN_PORT_FLOW, SIMPLEFLOW_MAXHOPCOUNT, DEFAULT_TOS);

  return p_brn;
}

void
BRN2SimpleFlow::reset()
{
  for (BRN2SimpleFlow::FMIter fm = _tx_flowMap.begin(); fm.live(); ++fm) {
    BRN2SimpleFlow::Flow *fl = fm.value();
    delete fl;
  }
  _tx_flowMap.clear();

  for (BRN2SimpleFlow::FMIter fm = _rx_flowMap.begin(); fm.live(); ++fm) {
    BRN2SimpleFlow::Flow *fl = fm.value();
    delete fl;
  }
  _rx_flowMap.clear();
}

/****************************************************************/
/********************** R E U S E *******************************/
/****************************************************************/

void
BRN2SimpleFlow::handle_reuse(Packet *packet)
{
  struct flowPacketHeader *header = (struct flowPacketHeader *)packet->data();
  uint32_t flow_id = ntohl(header->flowID);
  EtherAddress src_ea = EtherAddress(header->src);

  Flow *f_tx = _tx_flowMap.find(FlowID(src_ea, flow_id));

  if (!is_active(f_tx)) {
    if (f_tx == NULL) BRN_ERROR("Flow is NULL: %s:%d",src_ea.unparse().c_str(),flow_id);
    packet->kill();
    return;
  }

  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(packet);

#ifdef SF_TX_ABORT
  if ((ceh != NULL) && (ceh->magic == WIFI_EXTRA_MAGIC)) {
    if ((ceh->flags & WIFI_EXTRA_TX_FAIL) ) {
      BRN_ERROR("Failure: Retries: %d Max retries: %d",(int)ceh->retries,(int)ceh->max_tries);
    }
  }
#endif

  if ( f_tx->_header_size == -1 ) {
    if ( _headersize != -1 ) f_tx->_header_size = _headersize;
    else {
      f_tx->_header_size = sizeof(struct click_brn);
      if ( (ceh != NULL) && (ceh->magic == WIFI_EXTRA_MAGIC) ) f_tx->_header_size += sizeof(struct click_llc) + sizeof(struct click_wifi);
      else f_tx->_header_size += sizeof(struct click_ether);
      BRN_ERROR("ExtraSize: %d",f_tx->_header_size);
    }
  }

  if ( f_tx->_tx_packets_feedback == 0 ) f_tx->_first_feedback_time = Timestamp::now();
  f_tx->_last_feedback_time = Timestamp::now();

  if ((ceh != NULL) && (ceh->magic == WIFI_EXTRA_MAGIC)) {
    if (!(ceh->flags & WIFI_EXTRA_TX_FAIL) ) {
      f_tx->_tx_packets_feedback++;
    }
  }

  if ( f_tx->_buffered_p == NULL ) {
    if ( ceh != NULL ) {
      memset(ceh, 0, sizeof(struct click_wifi_extra));
      ceh->magic = WIFI_EXTRA_MAGIC;
    }
    f_tx->_buffered_p = packet;
  } else packet->kill();

  if ( (f_tx->_interval == 0) && ( is_active(f_tx)) ) {
    WritablePacket *p = nextPacketforFlow(f_tx);
    if ( p != NULL ) output(0).push(p);
  }
}

/****************************************************************************/
/********************** R O U T I N G P E E K *******************************/
/****************************************************************************/

bool
BRN2SimpleFlow::handle_routing_peek(Packet *p, EtherAddress */*src*/, EtherAddress */*dst*/, int /*brn_port*/)
{
  uint16_t checksum;

  // Here be dragons...

  p->pull(sizeof(struct click_brn));

  const click_ether *ether = (const click_ether *)p->ether_header();
  EtherAddress psrc = EtherAddress(ether->ether_shost);
  EtherAddress pdst = EtherAddress(ether->ether_dhost);

  if ( (_link_table != NULL) && (_link_table->_node_identity != NULL) ) {
    if (pdst.is_broadcast() || !_link_table->_node_identity->isIdentical((uint8_t*)ether->ether_dhost))
      pdst = *(_link_table->_node_identity->getMasterAddress());

    BRN_DEBUG("New Dst: %s", pdst.unparse().c_str());
  } else {
    BRN_DEBUG("Peek: keep bcast");
  }

  struct flowPacketHeader *header = (struct flowPacketHeader *)p->data();
  uint16_t size = p->length();

  BRN_DEBUG("RoutingPeek: FlowID %d Packet: %d", ntohl(header->flowID), ntohl(header->packetID));

  BRN_DEBUG("call of handle_routing_peek(): %d %d %d",(uint32_t)header->flags,
                                                      (header->flags|SIMPLEFLOW_FLAGS_IS_REPLY)?1:0
                                                      ,size);

  BRN_DEBUG("last hop: %s %s",psrc.unparse().c_str(),pdst.unparse().c_str());

  if ( (((uint32_t)header->flags & (uint32_t)SIMPLEFLOW_FLAGS_INCL_ROUTE) != 0) &&
       ((header->flags | SIMPLEFLOW_FLAGS_IS_REPLY) == 0) &&                        // no reply
       (size >= (MINIMUM_FLOW_PACKET_SIZE + header->extra_data_size + sizeof(struct flowPacketRoute))) ) {
    struct flowPacketRoute *rh = (struct flowPacketRoute *)&(((uint8_t*)&header[1])[header->extra_data_size]);

    BRN_DEBUG("Incl route");

    uint16_t hops = ntohs(rh->hops) + 1;
    uint16_t metric = 0;
    if ( _link_table != NULL ) metric = _link_table->get_link_metric(psrc, pdst);

    if ( size >=
         (MINIMUM_FLOW_PACKET_SIZE + header->extra_data_size + sizeof(struct flowPacketRoute) + (hops+1)*sizeof(struct flowPacketHop))) {

      BRN_DEBUG("Add hop: %d", metric);

      struct flowPacketHop *pfh = (struct flowPacketHop *)&rh[1];
      pfh[hops].metric = htons(metric);
      memcpy(pfh[hops].ea, pdst.data(),6);
      rh->incl_hops = htons(ntohs(rh->incl_hops)+1);
    }

    rh->hops = htons(hops);
    rh->metric_sum = htonl(ntohl(rh->metric_sum) + metric);

#if HAVE_FAST_CHECKSUM
    checksum = ip_fast_csum((unsigned char *)p->data() + (2 * sizeof(uint16_t)),
                             (p->length() - (2 * sizeof(uint16_t))) >> 2 );
#else
    checksum = click_in_cksum((unsigned char *)p->data() + (2 * sizeof(uint16_t)),
                                 (p->length() - (2 * sizeof(uint16_t))));
#endif
    header->crc = htons(checksum);
  }

  p = p->push(sizeof(struct click_brn));

  return true;
}

void
BRN2SimpleFlow::add_flow(String conf)
{
  Vector<String> args;

  String s = cp_uncomment(conf);
  cp_spacevec(s, args);

  if ( args.size() < 7 ) {
    BRN_WARN("Use: src dst interval size mode duration active [num_of_burst_pck [start_delay]]");
    BRN_WARN("You send. %s",conf.c_str());
  }

  EtherAddress src;
  EtherAddress dst;

  uint32_t interval;
  uint32_t size;
  uint32_t mode;
  uint32_t duration;
  bool active;
  uint32_t burst = 1;
  int32_t  start_delay = 0;

  BRN_DEBUG("ARGS: %s %s",args[0].c_str(), args[1].c_str());

  if ( !cp_ethernet_address(args[0], &src)) {
    BRN_DEBUG("Error. Use nameinfo");
    if (NameInfo::query(NameInfo::T_ETHERNET_ADDR, this, args[0], &src, 6))
      BRN_DEBUG("Found address!");
  }

  cp_integer(args[2], &interval);
  cp_integer(args[3], &size);
  cp_integer(args[4], &mode);
  cp_integer(args[5], &duration);
  cp_bool(args[6], &active);

  if ( args.size() > 7 ) cp_integer(args[7], &burst);
  if ( args.size() > 8 ) cp_integer(args[8], &start_delay);

  BRN_DEBUG("Group?");
  Vector<String> group;

  String group_string = args[1].unshared();
  str_replace(group_string, ',', ' ');
  str_replace(group_string, '+', ' ');
  cp_spacevec(group_string, group);

  for ( int i = 0; i < group.size(); i++ ) {
    if ( !cp_ethernet_address(group[i], &dst)) {
      BRN_DEBUG("Error. Use nameinfo");
      if (NameInfo::query(NameInfo::T_ETHERNET_ADDR, this, group[i], &dst, 6))
        BRN_DEBUG("Found address!");
    }

    add_flow( src, dst, size, mode, interval, burst, duration, active, start_delay);
  }
}

void BRN2SimpleFlow::set_extra_data(String new_extra_data_content)
{
  _extra_data = new_extra_data_content;
}


/****************************************************************************/
/********************** H A N D L E R   *************************************/
/****************************************************************************/

String
BRN2SimpleFlow::xml_stats()
{
  StringAccum sa;
  Timestamp now = Timestamp::now();

  sa << "<flowstats node=\"" << BRN_NODE_NAME << "\" time=\"" << now.unparse() << "\" sf_elem_id=\"" << _simpleflow_element_id << "\" >\n";
  for (BRN2SimpleFlow::FMIter fm = _tx_flowMap.begin(); fm.live(); ++fm) {
    BRN2SimpleFlow::Flow *fl = fm.value();
    sa << "\t<txflow src=\"" << fl->_src.unparse().c_str();
    sa << "\" dst=\"" << fl->_dst.unparse().c_str() << "\" flowid=\"" << fl->_id;
    sa << "\" packet_count=\"" << fl->_txPackets << "\" packet_size=\"" << fl->_size;
    sa << "\" replies=\"" << fl->_rxPackets << "\" tx_feedbacks=\"" << fl->_tx_packets_feedback;
    sa << "\" tx_rate=\"" << fl->get_txfeedback_rate() << "\" tx_rate_unit=\"bits/s\" start_time=\"";
    sa << fl->_start_time.unparse() << "\" end_time=\"" << fl->_end_time.unparse() << "\"";

    if ( fl->_rxPackets > 0 ) {
      sa << " min_hops=\"" << fl->_min_hops << "\" max_hops=\"" << fl->_max_hops;
      sa << "\" avg_hops=\"" << fl->_cum_sum_hops/fl->_rxPackets << "\" std_hops=\"" << fl->std_hops();
      sa << "\" min_time=\"" << fl->_min_rt_time  << "\" max_time=\"" << fl->_max_rt_time;
      sa << "\" time=\"" << fl->_cum_sum_rt_time/fl->_rxPackets << "\" std_time=\"" << fl->std_time() << "\"";
    } else {
      sa << " min_hops=\"0\" max_hops=\"0\" avg_hops=\"0\" std_hops=\"0\"";
      sa << " min_time=\"0\" max_time=\"0\" time=\"0\" std_time=\"0\"";
    }

    sa << " interval=\"" << fl->_interval << "\" burst=\"" << fl->_burst;
    sa << "\" extra_data=\"" << fl->_extra_data << "\" />\n";
  }
  for (BRN2SimpleFlow::FMIter fm = _rx_flowMap.begin(); fm.live(); ++fm) {
    BRN2SimpleFlow::Flow *fl = fm.value();
    sa << "\t<rxflow";
    sa << " src=\"" << fl->_src.unparse().c_str() << "\"";
    sa << " dst=\"" << fl->_dst.unparse().c_str() << "\" flowid=\"" << fl->_id << "\"";
    sa << " packet_count=\"" << fl->_rxPackets << "\" packet_size=\"" << fl->_size << "\"";
    sa << " max_packet_id=\"" << fl->_max_packet_id;
    sa << "\" out_of_order=\"" << fl->_rx_out_of_order << "\"";
    sa << " crc_err=\"" << fl->_rxCrcErrors << "\"";
    if ( fl->_rxPackets > 0 ) {
      sa << " min_hops=\"" << fl->_min_hops << "\" max_hops=\"" << fl->_max_hops;
      sa << "\" avg_hops=\"" << fl->_cum_sum_hops/fl->_rxPackets << "\" std_hops=\"" << fl->std_hops();
      sa << "\" min_time=\"" << fl->_min_rt_time  << "\" max_time=\"" << fl->_max_rt_time;
      sa << "\" time=\"" << fl->_cum_sum_rt_time/fl->_rxPackets << "\" std_time=\"" << fl->std_time() << "\"";
    } else {
      sa << " min_hops=\"0\" max_hops=\"0\" avg_hops=\"0\" std_hops=\"0\"";
      sa << " min_time=\"0\" max_time=\"0\" time=\"0\" std_time=\"0\"";
    };

    sa << " extra_data=\"" << fl->_extra_data << "\" >\n";

    if ( fl->route.size() != 0 ) {
      uint16_t sum_metric = 0;
      for ( int h = 0; h < fl->metric.size(); h++) sum_metric += fl->metric[h];
      sa << "\t\t<flowroute hops=\"" << fl->route_hops << "\" metric=\"" << fl->metric_sum;
      sa << "\" incl_hops=\"" << (fl->route.size()-1) << "\" incl_metric=\"" << sum_metric << "\" >\n";

      for ( int h = 0; h < fl->metric.size()-1; h++) {
        sa << "\t\t\t<flowhops from=\"" << fl->route[h] << "\" to=\"" << fl->route[h+1] << "\" metric=\"" << fl->metric[h+1] << "\" />\n";
      }
      sa << "\t\t</flowroute>\n";

    }

    sa << "\t</rxflow>\n";
  }
  sa << "</flowstats>\n";
  return sa.take_string();
}

enum {
  H_FLOW_STATS,
  H_ADD_FLOW,
  H_DEL_FLOW,
  H_RESET,
  H_EXTRA_DATA
};

static String
BRN2SimpleFlow_read_param(Element *e, void *thunk)
{
  BRN2SimpleFlow *sf = static_cast<BRN2SimpleFlow *>(e);

  switch ((uintptr_t) thunk) {
    case H_FLOW_STATS: {
      return sf->xml_stats();
    }
    default:
      return String();
  }
}

static int
BRN2SimpleFlow_write_param(const String &raw_params, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  BRN2SimpleFlow *sf = static_cast<BRN2SimpleFlow *>(e);
  String params = cp_uncomment(raw_params);
  switch((long)vparam) {
    case H_RESET: {
      sf->reset();
      break;
    }
    case H_ADD_FLOW: {
      sf->add_flow(params);
      break;
    }
    case H_DEL_FLOW: {
      break;
    }
    case H_EXTRA_DATA: {
      sf->set_extra_data(params);
      break;
    }
  }
  return 0;
}


void BRN2SimpleFlow::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", BRN2SimpleFlow_read_param, (void *)H_FLOW_STATS);

  add_write_handler("reset", BRN2SimpleFlow_write_param, (void *)H_RESET);
  add_write_handler("add_flow", BRN2SimpleFlow_write_param, (void *)H_ADD_FLOW);
  add_write_handler("del_flow", BRN2SimpleFlow_write_param, (void *)H_DEL_FLOW);
  add_write_handler("extra_data", BRN2SimpleFlow_write_param, (void *)H_EXTRA_DATA);
}

EXPORT_ELEMENT(BRN2SimpleFlow)
CLICK_ENDDECLS
