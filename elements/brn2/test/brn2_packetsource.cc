#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_packetsource.hh"

CLICK_DECLS

BRN2PacketSource::BRN2PacketSource()
  : _active(true),
    _timer(this)
{
}

BRN2PacketSource::~BRN2PacketSource()
{

}

int
BRN2PacketSource::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "SIZE", cpkP+cpkM, cpInteger, &_size,
      "INTERVAL", cpkP+cpkM, cpInteger, &_interval,
      "MAXSEQ", cpkP+cpkM, cpInteger, &_max_seq_num,
      "CHANNEL", cpkP+cpkM, cpInteger, &_channel,
      "BITRATE", cpkP+cpkM, cpInteger, &_bitrate,
      "POWER", cpkP+cpkM, cpInteger, &_power,
      "ACTIVE", cpkP, cpBool, &_active,
      cpEnd) < 0)
        return -1;
 
  return 0;
}

int
BRN2PacketSource::initialize(ErrorHandler *)
{
  click_random_srandom();

  if ( _interval > 0 ) {
    _timer.initialize(this);
    _timer.schedule_after_msec(_interval + ( click_random() % _interval ) );

    _seq_num = 1;
  }
  return 0;
}

void
BRN2PacketSource::run_timer(Timer *t)
{
  Packet *packet_out;

  if ( t == NULL ) click_chatter("Timer is NULL");

  _timer.reschedule_after_msec(_interval);

  if ( _active ) {
    packet_out = createpacket(_size);

    if ( ( _max_seq_num != 0 ) && ( _seq_num == _max_seq_num ) )
    _seq_num = 1;
    else
    _seq_num++;

    output(0).push(packet_out);
  }
}

void BRN2PacketSource::push( int port, Packet *packet )
{
  if ( port == 0 )
    output(0).push(packet);
  else
    packet->kill();

}

Packet *
BRN2PacketSource::createpacket(int size)
{
  //WritablePacket *new_packet = WritablePacket::make(size);
  WritablePacket *new_packet = WritablePacket::make(64 /*headroom*/,NULL /* *data*/, size, 32);
  uint8_t *new_packet_data = (uint8_t*)new_packet->data();

  uint16_t pshort;
  struct packetinfo newpi;

  memset(new_packet_data,0,size);

  newpi.seq_num = htonl(_seq_num);
  pshort=_interval;
  newpi.interval = /*htons(*/pshort/*)*/;
  newpi.channel = (uint8_t)_channel;
  newpi.bitrate = (uint8_t)_bitrate;
  newpi.power = (uint8_t)_power;

  //memcpy(&(new_packet_data[2]), &newpi, sizeof(struct packetinfo));

  memcpy(&(new_packet_data[2]), &newpi.seq_num, sizeof(uint32_t));
  memcpy(&(new_packet_data[2 + sizeof(uint32_t)]), &newpi.interval, sizeof(uint16_t));
  memcpy(&(new_packet_data[2 + sizeof(uint32_t) + sizeof(uint16_t) ]), &newpi.channel, sizeof(uint8_t));
  memcpy(&(new_packet_data[2 + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) ]), &newpi.bitrate, sizeof(uint8_t));
  memcpy(&(new_packet_data[2 + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) ]), &newpi.power, sizeof(uint8_t));

  return(new_packet);
}

enum {
  H_ACTIVE,
};

static String
read_param(Element *e, void *thunk)
{
  BRN2PacketSource *ps = (BRN2PacketSource *)e;

  switch ((uintptr_t) thunk) {
    case H_ACTIVE: {
      StringAccum sa;
      sa << ps->_active << "\n";
      return sa.take_string();
    }
    default:
      return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  BRN2PacketSource *ps = (BRN2PacketSource *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
    case H_ACTIVE: {
      Vector<String> args;
      cp_spacevec(in_s, args);

      bool a;
      cp_bool(args[0] ,&a);
      ps->_active = a;
      break;
    }
  }

  return 0;
}


void
BRN2PacketSource::add_handlers()
{
  add_read_handler("active", read_param, H_ACTIVE);
  add_write_handler("active", write_param, H_ACTIVE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2PacketSource)
