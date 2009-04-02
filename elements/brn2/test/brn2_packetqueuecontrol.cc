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
  : _timer(this),_queue_size_handler(0)
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
      "SIZE", cpkP+cpkM, cpInteger, &_packet_size,
      "MINP", cpkP+cpkM, cpInteger, &_min_count_p,
      "MAXP", cpkP+cpkM, cpInteger, &_max_count_p,  //TODO: read maxp using queue handler
      cpEnd) < 0)
        return -1;

  _interval = 10;

  return 0;
}

int
BRN2PacketQueueControl::initialize(ErrorHandler *errh)
{
  if( _queue_size_handler->initialize_read(this, errh) < 0 )
    return errh->error("Could not initialize read handler");

  if ( _interval > 0 ) {
    _timer.initialize(this);
    _timer.schedule_after_msec( 1000 );
  }
  return 0;
}

void
BRN2PacketQueueControl::run_timer(Timer *t)
{
  Packet *packet_out;

  if ( t == NULL ) click_chatter("Timer is NULL");

  int queue_size;
  String s_qsize = _queue_size_handler->call_read();
  cp_integer(s_qsize, &queue_size);

  click_chatter("Size:%d",queue_size);
  if ( queue_size < _min_count_p ) {
    for ( int i = 0; i < 20; i++) {
      packet_out = createpacket(_packet_size);
      output(0).push(packet_out);
    }
  }

  _timer.reschedule_after_msec(_interval);
}

Packet *
BRN2PacketQueueControl::createpacket(int size)
{
  WritablePacket *new_packet = WritablePacket::make(64 /*headroom*/,NULL /* *data*/, _packet_size, 32);
  return(new_packet);
}

static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
BRN2PacketQueueControl::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2PacketQueueControl)
