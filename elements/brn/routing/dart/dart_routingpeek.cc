#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <clicknet/ether.h>

#include "dart_protocol.hh"
#include "dart_routingpeek.hh"

CLICK_DECLS

DartRoutingPeek::DartRoutingPeek()
{
  RoutingPeek::init();
}

DartRoutingPeek::~DartRoutingPeek()
{
}

int DartRoutingPeek::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int DartRoutingPeek::initialize(ErrorHandler *)
{
  return 0;
}

void
DartRoutingPeek::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    uint32_t header_len = get_all_header_len(packet);

    BRN_DEBUG("P-len: %d  header_len: %d",packet->length(), header_len);

    if ( packet->length() > header_len ) {
      click_ether *ether = (click_ether *)&((const uint8_t*)packet->data())[header_len];
      uint16_t ethertype = ntohs(ether->ether_type);
      EtherAddress src = EtherAddress(ether->ether_shost);
      EtherAddress dst = EtherAddress(ether->ether_dhost);

      BRN_DEBUG("Got Ethertype: %X",ethertype);

      if ( ethertype == ETHERTYPE_BRN ) {
        packet->pull(header_len + 14);
        struct click_brn *brnh = (struct click_brn *)packet->data();
        if ( call_routing_peek(packet, &src, &dst, brnh->dst_port) ) {
          WritablePacket *p_out = packet->push(header_len + 14);
          output(0).push(p_out);
        }
        return;
      }
    }
    output(0).push(packet);
  }
}

uint32_t
DartRoutingPeek::get_all_header_len(Packet *packet)
{
  return (sizeof(click_brn) + sizeof(struct dart_routing_header));
}

void
DartRoutingPeek::add_handlers()
{
  RoutingPeek::add_handlers();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RoutingPeek)
EXPORT_ELEMENT(DartRoutingPeek)
