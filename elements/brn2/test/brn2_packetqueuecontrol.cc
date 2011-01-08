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
  : _queue_timer(static_queue_timer_hook, (void*)this),
    _flow_timer(static_flow_timer_hook, (void*)this),
    _queue_size_handler(0),
    _queue_reset_handler(0),
    _suppressor_active_handler(0),
    ac_flow(0),
    _flow_id(0),
    _packetheadersize(DEFAULT_PACKETHEADERSIZE)
{
  BRNElement::init();
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
      "SUPPRESSORHANDLER", cpkP+cpkM, cpHandlerCallPtrWrite, &_suppressor_active_handler,
      "MINP", cpkP+cpkM, cpInteger, &_min_count_p,
      "MAXP", cpkP+cpkM, cpInteger, &_max_count_p,  //TODO: read maxp using queue handler
      "DEBUG", cpkP, cpInteger, &_debug,
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
  BRN2PacketQueueControl::Flow *acflow = fc->get_flow();

  if ( acflow && acflow->_running ) fc->handle_queue_timer();
}

void
BRN2PacketQueueControl::static_flow_timer_hook(Timer *, void *v) {
  BRN2PacketQueueControl *fc = (BRN2PacketQueueControl *)v;
  //click_chatter("Flow timer");
  if ( fc->get_flow() ) fc->handle_flow_timer();
}

void
BRN2PacketQueueControl::handle_queue_timer()
{
  Packet *packet_out;

  uint32_t queue_size;
  String s_qsize = _queue_size_handler->call_read();
  cp_integer(s_qsize, &queue_size);

  BRN_DEBUG("Queue size:%d",queue_size);
  if ( queue_size < _min_count_p ) {
    if ( queue_size == 0 ) ac_flow->_queue_empty++;
    for ( uint32_t i = 0; i < (_max_count_p - queue_size); i++) {
      ac_flow->_send_packets++;
      packet_out = create_packet( ac_flow->_packetsize );
      output(0).push(packet_out);
    }
  }

  _queue_timer.schedule_after_msec(ac_flow->_interval);
}

Packet *
BRN2PacketQueueControl::create_packet(int size)
{
  WritablePacket *new_packet = WritablePacket::make(128 ,NULL , size, 32);
  return(new_packet);
}

void
BRN2PacketQueueControl::handle_flow_timer() {
  BRN_DEBUG("Flow timer handler");
  if ( ac_flow == NULL ) return;

  BRN_DEBUG("Found flow");

  if ( ! ac_flow->_running ) {

    ac_flow->_running = true;
    ac_flow->_start_ts = Timestamp::now();

    if ( _suppressor_active_handler != NULL ) {
      _suppressor_active_handler->call_write(String("true"),ErrorHandler::default_handler());
    } else {
      for ( uint32_t i = 0; i < _max_count_p; i++) {
        ac_flow->_send_packets++;
        Packet *packet_out = create_packet(ac_flow->_packetsize);
        output(0).push(packet_out);
      }
    }

    BRN_DEBUG("Schedule end of flow in %d ms",(ac_flow->_end - ac_flow->_start));
    _queue_timer.schedule_after_msec(ac_flow->_interval);
    _flow_timer.schedule_after_msec(ac_flow->_end - ac_flow->_start);

  } else {
    BRN_DEBUG("End of flow.");

    if ( _suppressor_active_handler != NULL ) {
      _suppressor_active_handler->call_write(String("false"),ErrorHandler::default_handler());
    }

    ac_flow->_running = false;

    int queue_size;
    String s_qsize = _queue_size_handler->call_read();
    cp_integer(s_qsize, &queue_size);

    ac_flow->_send_packets -= queue_size;
    ac_flow->_end_ts = Timestamp::now();

    _queue_reset_handler->call_write(ErrorHandler::default_handler());
  }

}


void
BRN2PacketQueueControl::setFlow(Flow *f) {
  ac_flow = f;

  BRN_DEBUG("Start flow timer in %d ms",ac_flow->_start);
  _flow_timer.schedule_after_msec(ac_flow->_start);

  ac_flow->_id = ++_flow_id;

  if ( _suppressor_active_handler != NULL ) {
    _suppressor_active_handler->call_write(String("false"),ErrorHandler::default_handler());

    for ( uint32_t i = 0; i < _max_count_p; i++) {
      ac_flow->_send_packets++;
      Packet *packet_out = create_packet(ac_flow->_packetsize);
      output(0).push(packet_out);
    }
  }
}

String
BRN2PacketQueueControl::flow_stats()
{
  StringAccum sa;
  //TODO: using double doesn't work in kernelmode !! other ideas ??
  int rate = 8 * ac_flow->_send_packets * ( ac_flow->_packetsize + _packetheadersize );
  rate /= ( ( ac_flow->_end - ac_flow->_start ) / 1000 );

  sa << "<interferencegraphflow ";
  sa << "node=\"" << BRN_NODE_NAME << "\" ";
  sa << "starttime=\"" << ac_flow->_start_ts << "\" endtime=\"" << ac_flow->_end_ts << "\" ";
  sa << "duration=\"" << (ac_flow->_end-ac_flow->_start) <<  "\" send_packets=\"" << ac_flow->_send_packets << "\" ";
  sa << "packetsize=\"" << ac_flow->_packetsize + _packetheadersize << "\" ";
  sa << "rate=\"" << rate << "\" ";
  sa << "unit=\"bits per sec\" ";
  sa << "running=\"" << ac_flow->_running << "\" ";
  sa << "queue_empty_cnt=\"" << ac_flow->_queue_empty << "\" />";

  return sa.take_string();
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
                        /*bytes in queue */                           /*(bw in MBit/s ) bytes pro ms*/
      int interval = ( ( (f->_max_count_p-f->_min_count_p) * packetsize) / ( bw * 125 /* 1000000 / (8 * 1000 (ms))*/));
      //click_chatter("%s Int: %d Start: %d End: %d packetsize: %d bw: %d",
      //                 Timestamp::now().unparse().c_str(), interval,start,end,packetsize, bw);
      f->setFlow(new BRN2PacketQueueControl::Flow(start,end,packetsize,interval));

      break;
    }
  }

  return 0;
}

static String
read_handler(Element *e, void *thunk)
{
  BRN2PacketQueueControl *qc = (BRN2PacketQueueControl *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS: {
      return qc->flow_stats();
    }
    default:
      return String();
  }
}

void
BRN2PacketQueueControl::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("flow_stats", read_handler, H_STATS);
  add_write_handler("flow_insert", write_handler, H_INSERT);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2PacketQueueControl)
