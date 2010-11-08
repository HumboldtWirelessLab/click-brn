#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_hotspotsconnector.hh"

CLICK_DECLS

BRN2HotSpotsConnector::BRN2HotSpotsConnector()
  : _timer(this)
{
}

BRN2HotSpotsConnector::~BRN2HotSpotsConnector()
{

}

int
BRN2HotSpotsConnector::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "STARTOFFSET", cpkP+cpkM, cpInteger, /*"offset",*/ &_start_offset,
      "UPDATEINTERVAL", cpkP+cpkM, cpInteger, /*"interval",*/ &_renew_interval,
      "CLICKIP", cpkP+cpkM, cpIPAddress, /*"ip of click",*/ &_click_ip,
      "CLICKPORT", cpkP+cpkM, cpInteger, /*"port of click",*/ &_click_port,
      "PACKETIP", cpkP+cpkM, cpIPAddress, /*"ip of packet",*/ &_packet_ip,
      "PACKETPORT", cpkP+cpkM, cpInteger, /*"port of packet",*/ &_packet_port,
      cpEnd) < 0)
        return -1;

  return 0;
}

int
BRN2HotSpotsConnector::initialize(ErrorHandler *)
{
  click_random_srandom();

  _timer.initialize(this);
  _timer.schedule_after_msec( (_start_offset * 1000 ) + ( click_random() % 5000 ) );

  _state = 0;

  return 0;
}

void
BRN2HotSpotsConnector::run_timer(Timer *)
{
  Packet *packet_out;

  packet_out = createpacket();

  _state = 1;

  _timer.reschedule_after_msec(_renew_interval);

  output(0).push(packet_out);
}

Packet *
BRN2HotSpotsConnector::createpacket()
{
  WritablePacket *new_packet = WritablePacket::make(sizeof(struct hscinfo));
 // struct hscinfo info;

  uint8_t *data;
  uint16_t port;

  data = new_packet->data();
  
  /*port = (uint16_t)_click_port;
  info.port = htons(port);
  memcpy(&info.ipaddr,_click_ip.data(),4);
  port = (uint16_t)_packet_port;
  info.packetport = htons(port);
  memcpy(&info.packetipaddr,_packet_ip.data(),4); */
  
  memcpy(data,_click_ip.data(),4);
  port = (uint16_t)_click_port;
  port = htons(port);
  memcpy(&(data[4]),&port,2);
  memcpy(&(data[6]),_packet_ip.data(),4);
  port = (uint16_t)_packet_port;
  port = htons(port);
  memcpy(&(data[10]),&port,2);

//  memcpy(new_packet->data(),&info,sizeof(struct hscinfo));

  return(new_packet);
}

static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
BRN2HotSpotsConnector::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2HotSpotsConnector)
