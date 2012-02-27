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
  : _active(false),
    _timer(this),
    _burst(1),
    _channel(0),
    _bitrate(0),
    _power(0),
    _headroom(128),
    _max_packets(0),
    _send_packets(0),
    _reuse(false),
    _reuseoffset(0)
{
  BRNElement::init();
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
      "BURST", cpkP+cpkM, cpInteger, &_burst,
      "PACKETCOUNT", cpkP, cpInteger, &_max_packets,
      "CHANNEL", cpkP, cpInteger, &_channel,
      "BITRATE", cpkP, cpInteger, &_bitrate,
      "POWER", cpkP, cpInteger, &_power,
      "ACTIVE", cpkP, cpBool, &_active,
      "HEADROOM", cpkP, cpInteger, &_headroom,
      "REUSE", cpkP, cpBool, &_reuse,
      "REUSEOFFSET", cpkP, cpInteger, &_reuseoffset,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
        return -1;

  return 0;
}

int
BRN2PacketSource::initialize(ErrorHandler *)
{
  uint16_t pshort;

  pinfo.seq_num = 0;
  pshort=_interval;
  pinfo.interval = htons(pshort);
  pinfo.channel = (uint8_t)_channel;
  pinfo.bitrate = (uint8_t)_bitrate;
  pinfo.power = (uint8_t)_power;

  click_random_srandom();

  _timer.initialize(this);

  if ( _active && ( _interval > 0 )) {
    _seq_num = 1;
    _timer.schedule_after_msec(_interval);
  }

  return 0;
}

void
BRN2PacketSource::run_timer(Timer *t)
{
  Packet *packet_out;

  if ( t == NULL ) click_chatter("Timer is NULL");

  if ( _active ) {

    for ( uint32_t i = 0; (i < _burst) && ((_max_packets == 0)||(_max_packets>_send_packets)); i++,_send_packets++ ) {
      packet_out = createpacket(_size);

      if ( ( _max_seq_num != 0 ) && ( _seq_num == _max_seq_num ) )
      _seq_num = 1;
      else
      _seq_num++;

      output(0).push(packet_out);
    }

    if ( (( _max_packets == 0 ) || ( _max_packets > _send_packets )) && !_reuse ) {
      _timer.reschedule_after_msec(_interval);
    }
  }
}

void
BRN2PacketSource::set_active(bool set_active)
{
  if ( _active == set_active ) return;

  if ( set_active ) {
    if ( _interval <= 0 ) return;

    _seq_num = 1;
    _send_packets = 0;
    _timer.reschedule_after_msec(_interval);
  } else {
    _timer.clear();
  }

  _active = set_active;

}

void
BRN2PacketSource::push( int port, Packet *packet )
{
  if ( port == 0 ) {
    if ( ( _max_packets == 0 ) || ( _max_packets > _send_packets ) ) {
      WritablePacket *packet_out = packet->uniqueify();
      uint8_t *packet_data = (uint8_t*)&(((uint8_t*)packet_out->data())[_reuseoffset]);

      pinfo.seq_num = htonl(_seq_num);
      memcpy(packet_data, &pinfo, sizeof(struct packetinfo));

      _send_packets++;
      _seq_num++;

      if ( noutputs() > 1 )
        output(1).push(packet_out);
      else
        output(0).push(packet_out);

    } else {
      packet->kill();
    }
  } else
    packet->kill();
}

Packet *
BRN2PacketSource::createpacket(int size)
{
  WritablePacket *new_packet = BRNElement::packet_new(_headroom, NULL, size, 32);
  uint8_t *new_packet_data = (uint8_t*)new_packet->data();

  memset(new_packet_data,0,size);

  pinfo.seq_num = htonl(_seq_num);

  memcpy(new_packet_data, &pinfo, sizeof(struct packetinfo));

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
      ps->set_active(a);
      break;
    }
  }

  return 0;
}


void
BRN2PacketSource::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("active", read_param, H_ACTIVE);
  add_write_handler("active", write_param, H_ACTIVE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2PacketSource)
