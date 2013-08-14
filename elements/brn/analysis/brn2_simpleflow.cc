#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/packet.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brn2_simpleflow.hh"

CLICK_DECLS

BRN2SimpleFlow::BRN2SimpleFlow()
  : _timer(this),
    _clear_packet(true),
    _headroom(128),
    _start_active(false),
    _routing_peek(NULL),
    _flow_id(0),
    _link_table(NULL)
{
  BRNElement::init();
}

BRN2SimpleFlow::~BRN2SimpleFlow()
{
  for (BRN2SimpleFlow::FMIter fm = _tx_flowMap.begin(); fm.live(); ++fm) {
    BRN2SimpleFlow::Flow *fl = fm.value();
    delete fl;
  }
  for (BRN2SimpleFlow::FMIter fm = _rx_flowMap.begin(); fm.live(); ++fm) {
    BRN2SimpleFlow::Flow *fl = fm.value();
    delete fl;
  }
}

int BRN2SimpleFlow::configure(Vector<String> &conf, ErrorHandler *errh)
{
  EtherAddress _src = EtherAddress::make_broadcast();
  dst_of_flow = EtherAddress::make_broadcast();
  uint32_t     _rate;
  uint32_t     _mode;
  uint32_t     _size;
  uint32_t     _duration;

  if (cp_va_kparse(conf, this, errh,
      "SRCADDRESS", cpkP , cpEtherAddress, &_src,
      "DSTADDRESS", cpkP, cpEtherAddress, &dst_of_flow,
      "RATE", cpkP, cpInteger, &_rate,
      "SIZE", cpkP, cpInteger, &_size,
      "MODE", cpkP, cpInteger, &_mode,
      "DURATION", cpkP, cpInteger, &_duration,
      "ACTIVE", cpkP, cpBool, &_start_active,
      "CLEARPACKET", cpkP, cpBool, &_clear_packet,
      "HEADROOM", cpkP, cpInteger, &_headroom,
      "ROUTINGPEEK", cpkP, cpElement, &_routing_peek,
      "LT", cpkP, cpElement, &_link_table,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if ( ! _src.is_broadcast() )
    add_flow( _src, dst_of_flow, _rate, _size, _mode, _duration, false);
  else
    _start_active = false;

  return 0;
}

static bool routing_peek_func(void *e, Packet *p, EtherAddress *src, EtherAddress *dst, int brn_port)
{
  return ((BRN2SimpleFlow *)e)->handle_routing_peek(p, src, dst, brn_port);
}

int BRN2SimpleFlow::initialize(ErrorHandler *)
{
  //don't move this to configure, since BRNNodeIdenty is not configured
  //completely while configure this element, so set_active can cause
  //seg, fault, while calling BRN_DEBUG in set_active
  _timer.initialize(this);

  if (_start_active) set_active(&dst_of_flow,_start_active);

  if (_routing_peek) {
    if (-1 == _routing_peek->add_routing_peek(routing_peek_func, (void*)this, BRN_PORT_FLOW ))
      BRN_ERROR("Failed adding routing_peek_func()");
  }

  return 0;
}

void
BRN2SimpleFlow::run_timer(Timer *t)
{
  BRN_DEBUG("Run timer.");

  if ( t == NULL ) click_chatter("Timer is NULL");

  if ( is_active(&dst_of_flow) ) {

    BRN_DEBUG("Is active");

    Flow *txFlow = _tx_flowMap.find(dst_of_flow);
    if ( txFlow) {
      Packet *packet_out;

      BRN_DEBUG("Send Next");
      Timestamp now = Timestamp::now();
      if ( (txFlow->_duration != 0) &&
         (((now - txFlow->_start_time).msecval()+txFlow->_rate) >= txFlow->_duration)) {
        BRN_DEBUG("Last Packet");
        txFlow->_active = false;  //last packet for limited flow
      }

      packet_out = nextPacketforFlow(txFlow);

      output(0).push(packet_out);
    }

    schedule_next(&dst_of_flow);
  }
}

void
BRN2SimpleFlow::set_active(EtherAddress *dst, bool active)
{
  BRN_DEBUG("set_active");
  Flow *txFlow = _tx_flowMap.find(*dst);
  if ( active ) {
    if ( ! is_active(dst)  ) {
      BRN_DEBUG("flow actived");
      txFlow->_active = active;
      txFlow->_start_time = Timestamp::now();
      if ( txFlow->_rate == 0 ) {
        Packet *packet_out = nextPacketforFlow(txFlow);
        output(0).push(packet_out);
      } else {
        schedule_next(dst);
      }
    }
  } else {
    if ( is_active(dst)  ) {
      BRN_DEBUG("flow deactived");
      _timer.clear();
      txFlow->_active = active;
    }
  }
}

bool
BRN2SimpleFlow::is_active(EtherAddress *dst)
{
  Flow *txFlow = _tx_flowMap.find(*dst);
  if ( txFlow ) {
    Timestamp now = Timestamp::now();
    if ( txFlow->_active ) {
      if ( ((now - txFlow->_start_time).msecval() <= txFlow->_duration) ||
            (txFlow->_duration == 0)/*endless*/ ) {
        return true;
      } else {
        txFlow->_active = false;
      }
    }
  }

  return false;
}

void
BRN2SimpleFlow::schedule_next(EtherAddress *dst)
{
  Flow *txFlow = _tx_flowMap.find(*dst);

  if ( txFlow ) {
    if ( txFlow->_active && txFlow->_rate > 0 ) {
      BRN_DEBUG("Rate: %d Timestamp: %s",txFlow->_rate,
                Timestamp::make_msec(txFlow->_rate).unparse().c_str());
      BRN_DEBUG("run timer in %d ms", txFlow->_rate);

      _timer.schedule_after_msec(txFlow->_rate);
    } else {
      BRN_DEBUG("Flow not active. (Rate : %d)", txFlow->_rate);
    }
  } else {
    BRN_DEBUG("No flow.");
  }

  if (_timer.scheduled()) {
    BRN_DEBUG("Timer is scheduled at %s",_timer.expiry().unparse().c_str());
  } else {
    if ( txFlow->_active )
      BRN_WARN("Timer is not scheduled");
  }
}

void
BRN2SimpleFlow::add_flow( EtherAddress src, EtherAddress dst,
                          uint32_t rate, uint32_t size, uint32_t mode,
                          uint32_t duration, bool active )
{
  Flow *txFlow = _tx_flowMap.find(dst);

  if ( txFlow != NULL ) {
    if ( active ) txFlow->reset();
  } else {
    _tx_flowMap.insert(dst, new Flow(src, dst, _flow_id++, (FlowType)mode, rate, size, duration));
  }

  dst_of_flow = dst;
  set_active(&dst_of_flow, active);
}

/**
 * 
 * @param  
 * @param packet 
 */
void
BRN2SimpleFlow::push( int port, Packet *packet )
{
  if ( port == 1 ) {
    handle_reuse(packet);
    return;
  }

  struct flowPacketHeader *header = (struct flowPacketHeader *)packet->data();
  Timestamp send_time;
  send_time.assign(ntohl(header->tv_sec), ntohl(header->tv_usec));

  uint16_t checksum;

  /*Handle Reply*/
  if ( header->reply == 1 ) {
    EtherAddress dst_ea = EtherAddress(header->dst);
    Flow *f_tx = _tx_flowMap.find(dst_ea);
    if ( f_tx ) {
      BRN_DEBUG("Got reply");

      f_tx->add_rx_stats((uint32_t)(Timestamp::now() - send_time).msecval(),
                         (uint32_t)(SIMPLEFLOW_MAXHOPCOUNT - BRNPacketAnno::ttl_anno(packet)));

      packet->kill();
      return;
    }
  }

  /*Handle Packet*/
  EtherAddress src_ea = EtherAddress(header->src);
  Flow *f = _rx_flowMap.find(src_ea);
  uint32_t packet_id = ntohl(header->packetID);

  if ( f == NULL ) {  //TODO: shorten this
    _rx_flowMap.insert(src_ea, new Flow(src_ea, EtherAddress(header->dst),ntohl(header->flowID),
                       (flowType)header->mode, ntohl(header->rate), ntohs(header->size), 0) );
    f = _rx_flowMap.find(src_ea);
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

  if ( (header->flags & SIMPLEFLOW_FLAGS_INCL_ROUTE) &&
       (packet->length() >= (MINIMUM_FLOW_PACKET_SIZE + sizeof(struct flowPacketRoute))) ) {

    struct flowPacketRoute *rh = (struct flowPacketRoute *)&header[1];
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
    header->reply = 1;
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
    header->reply = 1;
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

/*WritablePacket*
BRN2SimpleFlow::sendFullAck(Packet *p, Flow *f)
{
}

WritablePacket*
BRN2SimpleFlow::sendSmallAck(Packet *p, Flow *f)
{
}

WritablePacket*
BRN2SimpleFlow::sendAck(Packet *p, Flow *f)
{
}
*/
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
    p = WritablePacket::make(_headroom ,NULL /* *data*/, size, 32);
  }
  if ( _clear_packet ) memset(p->data(),0,size);
  struct flowPacketHeader *header = (struct flowPacketHeader *)p->data();
  
  p->set_timestamp_anno(Timestamp::now());

  memcpy(header->src, f->_src.data(),6);
  memcpy(header->dst, f->_dst.data(),6);

  header->flowID = htonl(f->_id);
  header->packetID = htonl(f->_txPackets);
  if ( f->_txPackets == 0 ) {
    BRNPacketAnno::set_flow_ctrl_flags_anno(p, FLOW_CTRL_FLOW_START);
  } else if ( ! f->_active ) { //stop the flow
    BRNPacketAnno::set_flow_ctrl_flags_anno(p, FLOW_CTRL_FLOW_END);
  }

  f->_txPackets++;

  header->rate = htonl(f->_rate);
  header->size = htons(f->_size);
  header->mode = f->_type;
  header->reply = 0;

  Timestamp ts = Timestamp::now();
  header->tv_sec = htonl(ts.sec());
  header->tv_usec = htonl(ts.nsec());

  BRN_DEBUG("Mode: %d",(int)f->_type);

  header->reserved = 0;

  if ( size >= (MINIMUM_FLOW_PACKET_SIZE + sizeof(struct flowPacketRoute))) {
    BRN_DEBUG("Packet is big. Add route");
    header->flags |= SIMPLEFLOW_FLAGS_INCL_ROUTE;

    struct flowPacketRoute *rh = (struct flowPacketRoute *)&header[1];
    rh->hops = 0;
    rh->metric_sum = 0;
    rh->incl_hops = 0;

    BRN_DEBUG("Header: %d",(uint32_t)header->flags);

    if ( size >= (MINIMUM_FLOW_PACKET_SIZE + sizeof(struct flowPacketRoute) + sizeof(struct flowPacketHop))) {
      struct flowPacketHop *pfh = (struct flowPacketHop *)&rh[1];
      pfh[0].metric = 0;
      memcpy(pfh[0].ea, f->_src.data(),6);
    }
  }

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
BRN2SimpleFlow::handle_reuse(Packet *packet)
{
  struct flowPacketHeader *header = (struct flowPacketHeader *)packet->data();
  EtherAddress dst_ea = EtherAddress(header->dst);
  Flow *f_tx = _tx_flowMap.find(dst_ea);

  if ( f_tx->_buffered_p == NULL ) f_tx->_buffered_p = packet;
  else packet->kill();

  if ( f_tx->_rate == 0 ) {
    BRN_DEBUG("Rate is zero. Full pull");
    WritablePacket *p = nextPacketforFlow(f_tx);
    BRN_DEBUG("Use packet: %d", (f_tx->_buffered_p==NULL)?1:0);
    output(0).push(p);
  }
}

void
BRN2SimpleFlow::reset()
{
  _rx_flowMap.clear();
  _tx_flowMap.clear();
}

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

  BRN_DEBUG("call of handle_routing_peek(): %d %d %d",(uint32_t)header->flags,header->reply,size);
  BRN_DEBUG("last hop: %s %s",psrc.unparse().c_str(),pdst.unparse().c_str());

  if ( (((uint32_t)header->flags & (uint32_t)SIMPLEFLOW_FLAGS_INCL_ROUTE) != 0) &&
       (header->reply != 1 ) &&
       (size >= (MINIMUM_FLOW_PACKET_SIZE + sizeof(struct flowPacketRoute))) ) {
    struct flowPacketRoute *rh = (struct flowPacketRoute *)&header[1];

    BRN_DEBUG("Incl route");

    uint16_t hops = ntohs(rh->hops) + 1;
    uint16_t metric = 0;
    if ( _link_table != NULL ) metric = _link_table->get_link_metric(psrc, pdst);

    if ( size >=
         (MINIMUM_FLOW_PACKET_SIZE + sizeof(struct flowPacketRoute) + (hops+1)*sizeof(struct flowPacketHop))) {

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


/****************************************************************************/
/********************** H A N D L E R   *************************************/
/****************************************************************************/

String
BRN2SimpleFlow::xml_stats()
{
  StringAccum sa;

  sa << "<flowstats node=\"" << BRN_NODE_NAME << "\">\n";
  for (BRN2SimpleFlow::FMIter fm = _tx_flowMap.begin(); fm.live(); ++fm) {
    BRN2SimpleFlow::Flow *fl = fm.value();
    sa << "\t<txflow";
    sa << " src=\"" << fl->_src.unparse().c_str() << "\"";
    sa << " dst=\"" << fl->_dst.unparse().c_str() << "\" flowid=\"" << fl->_id << "\"";
    sa << " packet_count=\"" << fl->_txPackets << "\"";
    sa << " packet_size=\"" << fl->_size << "\"";
    sa << " replies=\"" << fl->_rxPackets << "\"";
    if ( fl->_rxPackets > 0 ) {
      sa << " min_hops=\"" << fl->_min_hops << "\" max_hops=\"" << fl->_max_hops;
      sa << "\" avg_hops=\"" << fl->_cum_sum_hops/fl->_rxPackets << "\" std_hops=\"" << fl->std_hops();
      sa << "\" min_time=\"" << fl->_min_rt_time  << "\" max_time=\"" << fl->_max_rt_time;
      sa << "\" time=\"" << fl->_cum_sum_rt_time/fl->_rxPackets << "\" std_time=\"" << fl->std_time() << "\" />\n";
    } else {
      sa << " min_hops=\"0\" max_hops=\"0\" avg_hops=\"0\" std_hops=\"0\"";
      sa << " min_time=\"0\" max_time=\"0\" time=\"0\" std_time=\"0\" />\n";
    }
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
      sa << "\" time=\"" << fl->_cum_sum_rt_time/fl->_rxPackets << "\" std_time=\"" << fl->std_time() << "\" >\n";
    } else {
      sa << " min_hops=\"0\" max_hops=\"0\" avg_hops=\"0\" std_hops=\"0\"";
      sa << " min_time=\"0\" max_time=\"0\" time=\"0\" std_time=\"0\" >\n";
    };

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
  H_FLOW_ACTIVE,
  H_ADD_FLOW,
  H_DEL_FLOW,
  H_RESET
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
BRN2SimpleFlow_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  BRN2SimpleFlow *sf = static_cast<BRN2SimpleFlow *>(e);
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
    case H_RESET: {
      sf->reset();
      break;
    }
    case H_FLOW_ACTIVE: {
      Vector<String> args;
      cp_spacevec(s, args);

      bool active;
      cp_bool(args[0], &active);

      sf->set_active(&(sf->dst_of_flow), active);
      break;
    }
    case H_ADD_FLOW: {
      Vector<String> args;
      cp_spacevec(s, args);

      if ( args.size() < 7 ) {
        click_chatter("Use: Src Dst rate size mode duration active");
        click_chatter("You send. %s",in_s.c_str());
      }
      EtherAddress src;
      EtherAddress dst;

      uint32_t rate;
      uint32_t size;
      uint32_t mode;
      uint32_t duration;
      bool active;

      //click_chatter("ARGS: %s %s",args[0].c_str(), args[1].c_str());
      cp_ethernet_address(args[0], &src);
      cp_ethernet_address(args[1], &dst);
      cp_integer(args[2], &rate);
      cp_integer(args[3], &size);
      cp_integer(args[4], &mode);
      cp_integer(args[5], &duration);
      cp_bool(args[6], &active);

      /** TODO: some valid checks */
      sf->add_flow( src, dst, rate, size, mode, duration, active);
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
  add_write_handler("active", BRN2SimpleFlow_write_param, (void *)H_FLOW_ACTIVE);
  add_write_handler("add_flow", BRN2SimpleFlow_write_param, (void *)H_ADD_FLOW);
  add_write_handler("del_flow", BRN2SimpleFlow_write_param, (void *)H_DEL_FLOW);
}

EXPORT_ELEMENT(BRN2SimpleFlow)
CLICK_ENDDECLS
