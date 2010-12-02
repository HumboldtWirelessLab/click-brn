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

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "brn2_simpleflow.hh"

CLICK_DECLS

BRN2SimpleFlow::BRN2SimpleFlow()
  : _timer(this),
    _clear_packet(false),
    _headroom(128)
{
  BRNElement::init();
}

BRN2SimpleFlow::~BRN2SimpleFlow()
{
}

int BRN2SimpleFlow::configure(Vector<String> &conf, ErrorHandler *errh)
{
  EtherAddress _dst;
  EtherAddress _src;
  uint32_t     _rate;
  uint32_t     _mode;
  uint32_t     _size;
  bool         _active;
  uint32_t     _duration;

  if (cp_va_kparse(conf, this, errh,
      "SRCADDRESS", cpkP+cpkM , cpEtherAddress, &_src,
      "DSTADDRESS", cpkP+cpkM, cpEtherAddress, &_dst,
      "RATE", cpkP+cpkM, cpInteger, &_rate,
      "SIZE", cpkP+cpkM, cpInteger, &_size,
      "MODE", cpkP+cpkM, cpInteger, &_mode,
      "DURATION", cpkP+cpkM, cpInteger, &_duration,
      "ACTIVE", cpkP+cpkM, cpBool, &_active,
      "CLEARPACKET", cpkP, cpBool, &_clear_packet,
      "HEADROOM", cpkP, cpInteger, &_headroom,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  add_flow( _src, _dst, _rate, _size, _mode, _duration, false);
  set_active(&_dst,_active);

  return 0;
}

int BRN2SimpleFlow::initialize(ErrorHandler *)
{
  click_srandom(dst_of_flow.hashcode());
  _timer.initialize(this);

  schedule_next(&dst_of_flow);

  return 0;
}

void
BRN2SimpleFlow::run_timer(Timer *t)
{
  BRN_DEBUG("Run timer.");

  Packet *packet_out;

  if ( t == NULL ) click_chatter("Timer is NULL");

  if ( is_active(&dst_of_flow) ) {
    schedule_next(&dst_of_flow);

    Flow *txFlow = _tx_flowMap.findp(dst_of_flow);
    if ( txFlow) {
      packet_out = nextPacketforFlow(txFlow);

      output(0).push(packet_out);
    }
  }
}

void
BRN2SimpleFlow::set_active(EtherAddress *dst, bool active)
{
  BRN_DEBUG("Flow active");
  Flow *txFlow = _tx_flowMap.findp(*dst);
  txFlow->_active = active;
}

bool
BRN2SimpleFlow::is_active(EtherAddress *dst)
{
  Flow *txFlow = _tx_flowMap.findp(*dst);
  if ( txFlow ) return txFlow->_active;

  return false;
}

void
BRN2SimpleFlow::schedule_next(EtherAddress *dst)
{
  Flow *txFlow = _tx_flowMap.findp(*dst);

  if ( txFlow ) {
    if ( txFlow->_active && txFlow->_rate > 0 ) {
      BRN_DEBUG("run timer in %d ms", txFlow->_rate);
      _timer.schedule_after_msec(txFlow->_rate);
    } else {
      BRN_DEBUG("Flow not active.");
    }
  } else {
    BRN_DEBUG("No flow.");
  }
}

void
BRN2SimpleFlow::add_flow( EtherAddress src, EtherAddress dst,
                          uint32_t rate, uint32_t size, uint32_t /*mode*/,
                          uint32_t duration, bool active )
{
  Flow *txFlow = _tx_flowMap.findp(dst);

  if ( txFlow != NULL ) {
    if ( active ) txFlow->reset();
  } else {
    _tx_flowMap.insert(dst, Flow(src, dst, 1, TYPE_NO_ACK, DIR_ME_RECEIVER, rate, size, duration));
  }

  dst_of_flow = dst;
  set_active(&dst_of_flow, active);

  if ( is_active(&dst_of_flow) ) schedule_next(&dst_of_flow);
}

void
BRN2SimpleFlow::push( int /*port*/, Packet *packet )
{
  struct flowPacketHeader *header = (struct flowPacketHeader *)packet->data();
  uint16_t checksum;

  EtherAddress src_ea = EtherAddress(header->src);
  Flow *f = _rx_flowMap.findp(src_ea);

  if ( f == NULL ) {  //TODO: shorten this
    _rx_flowMap.insert(src_ea, Flow(src_ea, EtherAddress(header->dst),ntohl(header->flowID), TYPE_NO_ACK, DIR_ME_RECEIVER, ntohl(header->rate), ntohl(header->size), 0) );
    f = _rx_flowMap.findp(src_ea);
  }

  f->_rxPackets++;

#if HAVE_FAST_CHECKSUM
  checksum = ip_fast_csum((unsigned char *)packet->data() + (2 * sizeof(uint16_t)), (packet->length() - (2 * sizeof(uint16_t))) >> 2 );
#else
  checksum = click_in_cksum((unsigned char *)packet->data() + (2 * sizeof(uint16_t)), (packet->length() - (2 * sizeof(uint16_t))));
#endif
  BRN_INFO("Insum: %d",checksum);

  if ( checksum != header->crc ) f->_rxCrcErrors++;

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

  p = WritablePacket::make(_headroom ,NULL /* *data*/, size, 32);
  if ( _clear_packet ) memset(p->data(),0,size);
  struct flowPacketHeader *header = (struct flowPacketHeader *)p->data();

  memcpy(header->src, f->_src.data(),6);
  memcpy(header->dst, f->_dst.data(),6);

  header->flowID = htonl(f->_id);
  header->packetID = htonl(f->_txPackets);
  f->_txPackets++;

  header->rate = htonl(f->_rate);
  header->size = htonl(f->_size);
  header->mode = htonl(f->_type);
  header->reply = 0;

#if HAVE_FAST_CHECKSUM
  checksum = ip_fast_csum((unsigned char *)p->data() + (2 * sizeof(uint16_t)), (p->length() - (2 * sizeof(uint16_t))) >> 2 );
#else
  checksum = click_in_cksum((unsigned char *)p->data() + (2 * sizeof(uint16_t)), (p->length() - (2 * sizeof(uint16_t))));
#endif
  BRN_INFO("Outsum: %d",checksum);

  header->crc = checksum;
  header->reserved = 0;

  BRNPacketAnno::set_ether_anno(p, f->_src, f->_dst, ETHERTYPE_BRN );
  p_brn = BRNProtocol::add_brn_header(p, BRN_PORT_FLOW, BRN_PORT_FLOW, 255, 0);

  return p_brn;
}

/****************************************************************************/
/********************** H A N D L E R   *************************************/
/****************************************************************************/

enum {
  H_TXFLOWS_SHOW,
  H_RXFLOWS_SHOW,
  H_FLOW_STATS,
  H_FLOW_ACTIVE,
  H_ADD_FLOW,
  H_DEL_FLOW
};

static String
BRN2SimpleFlow_read_param(Element *e, void *thunk)
{
  BRN2SimpleFlow *sf = (BRN2SimpleFlow *)e;

  switch ((uintptr_t) thunk) {
    case H_TXFLOWS_SHOW: {
      StringAccum sa;
      sa << "TxFlows:\n";
      for (BRN2SimpleFlow::FMIter fm = sf->_rx_flowMap.begin(); fm.live(); fm++) {
        BRN2SimpleFlow::Flow fl = fm.value();
        sa << " Destination: " << fl._dst.unparse().c_str();
        sa << " Packets: " << fl._txPackets;
        sa << "\n";
      }
      return sa.take_string();
    }
    case H_RXFLOWS_SHOW: {
      StringAccum sa;
      sa << "RxFlows:\n";
      for (BRN2SimpleFlow::FMIter fm = sf->_rx_flowMap.begin(); fm.live(); fm++) {
        BRN2SimpleFlow::Flow fl = fm.value();
        sa << "Source: " << fl._src.unparse().c_str();
        sa << " Packets: " << fl._rxPackets << " CRC-Errors: " << fl._rxCrcErrors;
        sa << "\n";
      }
      return sa.take_string();
    }
    case H_FLOW_STATS: {
      StringAccum sa;
      sa << "<flowstats node=\"" << "UNKNOWN" << "\">\n";
      for (BRN2SimpleFlow::FMIter fm = sf->_tx_flowMap.begin(); fm.live(); fm++) {
        BRN2SimpleFlow::Flow fl = fm.value();
        sa << "\t<txflow";
        sa << " src=\"" << fl._src.unparse().c_str() << "\"";
        sa << " dst=\"" << fl._dst.unparse().c_str() << "\"";
        sa << " packet_count=\"" << fl._txPackets << "\" />\n";
      }
      for (BRN2SimpleFlow::FMIter fm = sf->_rx_flowMap.begin(); fm.live(); fm++) {
        BRN2SimpleFlow::Flow fl = fm.value();
        sa << "\t<rxflow";
        sa << " src=\"" << fl._src.unparse().c_str() << "\"";
        sa << " dst=\"" << fl._dst.unparse().c_str() << "\"";
        sa << " packet_count=\"" << fl._rxPackets << "\" crc_err=\"" << fl._rxCrcErrors << "\" />\n";
      }
      sa << "</flowstats>\n";
      return sa.take_string();
    }
    default:
      return String();
  }
}

static int 
BRN2SimpleFlow_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  BRN2SimpleFlow *sf = (BRN2SimpleFlow *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
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

  add_read_handler("txflows", BRN2SimpleFlow_read_param, (void *)H_TXFLOWS_SHOW);
  add_read_handler("rxflows", BRN2SimpleFlow_read_param, (void *)H_RXFLOWS_SHOW);
  add_read_handler("stats", BRN2SimpleFlow_read_param, (void *)H_FLOW_STATS);

  add_write_handler("active", BRN2SimpleFlow_write_param, (void *)H_FLOW_ACTIVE);
  add_write_handler("add_flow", BRN2SimpleFlow_write_param, (void *)H_ADD_FLOW);
  add_write_handler("del_flow", BRN2SimpleFlow_write_param, (void *)H_DEL_FLOW);
}

EXPORT_ELEMENT(BRN2SimpleFlow)
#include <click/bighashmap.cc>
#include <click/vector.cc>
#include <click/dequeue.cc>

CLICK_ENDDECLS
