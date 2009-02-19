#include <click/config.h>
#include "brn2_simpleflow.hh"

#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/packet.hh>

#include "elements/brn/standard/brnpacketanno.hh"
#include "elements/brn/brn.h"

CLICK_DECLS

BRN2SimpleFlow::BRN2SimpleFlow()
  : _timer(this)
{
}

BRN2SimpleFlow::~BRN2SimpleFlow()
{
}

int BRN2SimpleFlow::configure(Vector<String> &conf, ErrorHandler *errh)
{
  EtherAddress _src;
  EtherAddress _dst;
  uint32_t     _rate;
  uint32_t     _mode;
  uint32_t     _size;
  uint32_t     _active;
  uint32_t     _duration;

  if (cp_va_kparse(conf, this, errh,
      "SRCADDRESS", cpkP+cpkM , cpEtherAddress, &_src,
      "DSTADDRESS", cpkP+cpkM, cpEtherAddress, &_dst,
      "RATE", cpkP+cpkM, cpInteger, &_rate,
      "SIZE", cpkP+cpkM, cpInteger, &_size,
      "MODE", cpkP+cpkM, cpInteger, &_mode,
      "DURATION", cpkP+cpkM, cpInteger, &_duration,
      "ACTIVE", cpkP+cpkM, cpInteger, &_active,
      cpEnd) < 0)
    return -1;

  txFlow = Flow(_src, _dst, 1, TYPE_SMALL_ACK, DIR_ME_SENDER, _rate, _size, _duration);
  if ( _active == 0 )
    txFlow._active = false;
  else
    txFlow._active = true;

  return 0;
}

int BRN2SimpleFlow::initialize(ErrorHandler *)
{
  if ( txFlow._rate > 0 ) {
    _timer.initialize(this);
    _timer.schedule_after_msec(txFlow._rate + ( random() % txFlow._rate ) );
  }

  return 0;
}

void
BRN2SimpleFlow::run_timer(Timer *t)
{
  Packet *packet_out;

  if ( t == NULL ) click_chatter("Timer is NULL");

  _timer.reschedule_after_msec(txFlow._rate);

  if ( txFlow._active ) {
    packet_out = nextPacketforFlow(&txFlow);
    output(0).push(packet_out);
  }
}

void
BRN2SimpleFlow::push( int /*port*/, Packet *packet )
{
  struct flowPacketHeader *header = (struct flowPacketHeader *)packet->data();

  EtherAddress src_ea = EtherAddress(header->src);
  Flow *f = _flowMap.findp(src_ea);

  if ( f == NULL ) {  //TODO: shorten this
    _flowMap.insert(src_ea, Flow(src_ea, EtherAddress(header->dst),ntohl(header->flowID), TYPE_NO_ACK, DIR_ME_RECEIVER, ntohl(header->rate), ntohl(header->size), 0) );
    f = _flowMap.findp(src_ea);
  }

  f->_rxPackets++;

  packet->kill();
}

WritablePacket*
BRN2SimpleFlow::nextPacketforFlow(Flow *f)
{
  WritablePacket *p;
  WritablePacket *p_brn;
  uint32_t size = f->_size;

  if ( size < MINIMUM_FLOW_PACKET_SIZE )  //TODO: warn that we extend the packet
    size = MINIMUM_FLOW_PACKET_SIZE;

  p = WritablePacket::make(64 /*headroom*/,NULL /* *data*/, size, 32);
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

  BRNPacketAnno::set_dst_ether_anno(p, f->_dst);
  BRNPacketAnno::set_src_ether_anno(p, f->_src);

  //TODO: use brnprotocol-function

  p_brn = p->push(sizeof(click_brn));
  click_brn *brn_header = (click_brn*)p_brn->data();

  brn_header->dst_port = BRN_PORT_FLOW;
  brn_header->src_port = BRN_PORT_FLOW;
  brn_header->body_length = htons(size);
  brn_header->ttl = 255;
  brn_header->tos = 0;

  return p_brn;
}

enum {
  H_FLOWS_SHOW,
  H_FLOW_ACTIVE
};

static String
BRN2SimpleFlow_read_param(Element */*e*/, void *thunk)
{
//  BRN2SimpleFlow *sf = (BRN2SimpleFlow *)e;

  switch ((uintptr_t) thunk) {
    case H_FLOWS_SHOW: {
      StringAccum sa;
      sa << "Flows";
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
      sf->set_active();
      break;
    }
  }

  return 0;
}

void BRN2SimpleFlow::add_handlers()
{
  add_read_handler("flow", BRN2SimpleFlow_read_param, (void *)H_FLOWS_SHOW);
  add_write_handler("active", BRN2SimpleFlow_write_param, (void *)H_FLOW_ACTIVE);
}

EXPORT_ELEMENT(BRN2SimpleFlow)
#include <click/bighashmap.cc>
#include <click/vector.cc>
#include <click/dequeue.cc>

CLICK_ENDDECLS
