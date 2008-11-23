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
  : _timer(this)
{
}

BRN2PacketSource::~BRN2PacketSource()
{

}

int
BRN2PacketSource::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "SIZE", cpkP+cpkM, cpInteger, /*"size",*/ &_size,
      "INTERVAL", cpkP+cpkM, cpInteger, /*"interval",*/ &_interval,
      "MAXSEQ", cpkP+cpkM, cpInteger, /*"_max-seq_num",*/ &_max_seq_num,
      "CHANNEL", cpkP+cpkM, cpInteger, /*"_channel",*/ &_channel,
      "BITRATE", cpkP+cpkM, cpInteger, /*"_bitrate",*/ &_bitrate,
      "POWER", cpkP+cpkM, cpInteger, /*"_power",*/ &_power,
      cpEnd) < 0)
        return -1;
 
  return 0;
}

int
BRN2PacketSource::initialize(ErrorHandler *)
{
  if ( _interval > 0 ) {
    _timer.initialize(this);
    _timer.schedule_after_msec(_interval + ( random() % _interval ) );

    _seq_num = 1;
  }
  return 0;
}

void
BRN2PacketSource::run_timer(Timer *t)
{
  Packet *packet_out;

  if ( t == NULL ) click_chatter("Timmer is NULL");

  _timer.reschedule_after_msec(_interval);

  packet_out = createpacket(_size);

  if ( ( _max_seq_num != 0 ) && ( _seq_num == _max_seq_num ) )
   _seq_num = 1;
  else
   _seq_num++;

  output(0).push(packet_out);
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

static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
BRN2PacketSource::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2PacketSource)
