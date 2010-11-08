#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_packetqueuecontrol.hh"

CLICK_DECLS

BRN2PacketQueueControl::BRN2PacketQueueControl()
  : _queue_timer(static_queue_timer_hook,this),
    _flow_timer(static_flow_timer_hook,this),
    _queue_size_handler(0),
    _queue_reset_handler(0),
    acflow(0)
{
}

BRN2PacketQueueControl::~BRN2PacketQueueControl()
{

}

int
BRN2PacketQueueControl::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "QUEUESIZEHANDLER", cpkP+cpkM, cpHandlerCallPtrRead, &_queue_size_handler,
      "QUEUERESETHANDLER", cpkP+cpkM, cpHandlerCallPtrWrite, &_queue_reset_handler,
      "MINP", cpkP+cpkM, cpInteger, &_min_count_p,
      "MAXP", cpkP+cpkM, cpInteger, &_max_count_p,  //TODO: read maxp using queue handler
      cpEnd) < 0)
        return -1;

  return 0;
}

int
BRN2PacketQueueControl::initialize(ErrorHandler *errh)
{
  if( _queue_size_handler->initialize_read(this, errh) < 0 )
    return errh->error("Could not initialize read handler");

  if( _queue_reset_handler->initialize_write(this, errh) < 0 )
    return errh->error("Could not initialize  handler");

  _queue_timer.initialize(this);
  _flow_timer.initialize(this);

  return 0;
}

void
BRN2PacketQueueControl::static_queue_timer_hook(Timer *, void *v) {
  BRN2PacketQueueControl *fc = (BRN2PacketQueueControl *)v;
  BRN2PacketQueueControl::Flow *acflow = fc->getAcFlow();
  if ( acflow->_running )
    fc->handle_queue_timer();
}

void
BRN2PacketQueueControl::static_flow_timer_hook(Timer *, void *v) {
  BRN2PacketQueueControl *fc = (BRN2PacketQueueControl *)v;
  fc->handle_flow_timer();
}

void
BRN2PacketQueueControl::handle_queue_timer()
{
  Packet *packet_out;

  uint32_t queue_size;
  String s_qsize = _queue_size_handler->call_read();
  cp_integer(s_qsize, &queue_size);

  click_chatter("Size:%d",queue_size);
  if ( queue_size < _min_count_p ) {
    for ( uint32_t i = 0; i < (_max_count_p - queue_size); i++) {
      acflow->_send_packets++;
      packet_out = createpacket( acflow->_packetsize );
      output(0).push(packet_out);
    }
  }

  _queue_timer.reschedule_after_msec(acflow->_interval);
}

Packet *
BRN2PacketQueueControl::createpacket(int size)
{
  WritablePacket *new_packet = WritablePacket::make(64 /*headroom*/,NULL /* *data*/, size, 32);
  return(new_packet);
}

void
BRN2PacketQueueControl::handle_flow_timer() {
  if ( acflow == NULL ) return;

  if ( ! acflow->_running ) {
    acflow->_running = true;
    for ( uint32_t i = 0; i < _max_count_p; i++) {
      acflow->_send_packets++;
      Packet *packet_out = createpacket( acflow->_packetsize );
      output(0).push(packet_out);
    }

    click_chatter("set timer for flowend");
    _queue_timer.reschedule_after_msec(acflow->_interval);
    _flow_timer.reschedule_after_msec(acflow->_end - acflow->_start);
    click_chatter("Schedule flowend in %d s",(acflow->_end - acflow->_start));
  } else {
    click_chatter("flowend");
    acflow->_running = false;
    int queue_size;
    String s_qsize = _queue_size_handler->call_read();
    cp_integer(s_qsize, &queue_size);

    acflow->_send_packets -= queue_size;

    _queue_reset_handler->call_write(ErrorHandler::default_handler());
  }

}


void
BRN2PacketQueueControl::setFlow(Flow *f) {
  acflow = f;
  _flow_timer.reschedule_after_msec(f->_start);
}


enum {
  H_INSERT,
  H_STATS
};

static int
write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *)
{
  BRN2PacketQueueControl *f = (BRN2PacketQueueControl *)e;
  String s = cp_uncomment(in_s);

  switch((intptr_t)vparam) {
    case H_INSERT: {
      Vector<String> args;
      cp_spacevec(s, args);

      int start,end,packetsize,bw;

      cp_integer(args[0], &start);
      cp_integer(args[1], &end);
      cp_integer(args[2], &packetsize);
      cp_integer(args[3], &bw);

      int interval = ( ((f->_max_count_p-f->_min_count_p) * 1000) / (bw*1000*125 /* 1000/8 */ /packetsize));
      click_chatter("Int: %d",interval);
      f->setFlow(new BRN2PacketQueueControl::Flow(start,end,packetsize,interval));

      break;
    }
  }

  return 0;
}

static String
read_handler(Element *e, void *thunk)
{
  BRN2PacketQueueControl *f = (BRN2PacketQueueControl *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS: {
      StringAccum sa;
      BRN2PacketQueueControl::Flow *acflow = f->getAcFlow();
      //TODO: using double doesn'z work in kernelmode !! other ideas ??
      int rate = acflow->_send_packets * acflow->_packetsize;
      rate /= ( ( acflow->_end - acflow->_start ) / 1000 );
      sa << "Packets: " << acflow->_send_packets;
      sa << "\nRate: " << rate;
      sa << " Byte/s\n";
      sa << "Running : ";
      if ( acflow->_running ) sa << "yes";
      else sa << "no";
      return sa.take_string();
    }
    default:
      return String();
  }
}

void
BRN2PacketQueueControl::add_handlers()
{
  add_read_handler("flow_stats", read_handler, H_STATS);
  add_write_handler("flow_insert", write_handler, H_INSERT);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2PacketQueueControl)
