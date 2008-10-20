#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brncanseltest.hh"

CLICK_DECLS

BRNCandidateSelectorTest::BRNCandidateSelectorTest()
  : _timer(this)
{
}

BRNCandidateSelectorTest::~BRNCandidateSelectorTest()
{

}

int
BRNCandidateSelectorTest::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
      cpOptional,
      cpInteger, "size", &_size,
      cpInteger, "interval", &_interval,
      cpEnd) < 0)
        return -1;
 
  return 0;
}

int
BRNCandidateSelectorTest::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_msec(_interval + ( random() % _interval ) );

  return 0;
}

void
BRNCandidateSelectorTest::run_timer(Timer *t)
{
  Packet *packet_out;

  _timer.reschedule_after_msec(_interval);

  packet_out = createpacket(_size);

  output(0).push(packet_out);
}

Packet *
BRNCandidateSelectorTest::createpacket(int size)
{
  WritablePacket *new_packet = WritablePacket::make(size);
  uint8_t *new_packet_data = (uint8_t*)new_packet->data();
  uint16_t proto = 0x8680;
  uint16_t brnproto = 0x0909;

  memset(new_packet_data,0,size);

  memcpy( &(new_packet_data[12]), &proto, sizeof(uint16_t) ) ;
  memcpy( &(new_packet_data[14]), &brnproto, sizeof(uint16_t) ) ;

  click_ether *ether = (click_ether *)new_packet->data();

  return(new_packet);
}

static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
BRNCandidateSelectorTest::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNCandidateSelectorTest)
