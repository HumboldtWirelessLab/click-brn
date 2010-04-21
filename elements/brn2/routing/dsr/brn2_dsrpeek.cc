#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <clicknet/ether.h>

#include "brn2_dsrencap.hh"
#include "brn2_dsrdecap.hh"

#include "brn2_dsrpeek.hh"

CLICK_DECLS

DSRPeek::DSRPeek()
{
  RoutingPeek::init();
}

DSRPeek::~DSRPeek()
{
}

int DSRPeek::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int DSRPeek::initialize(ErrorHandler *)
{
  return 0;
}

void
DSRPeek::push( int port, Packet *packet )
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
DSRPeek::get_all_header_len(Packet *packet)
{
  click_brn_dsr *dsr_src = (click_brn_dsr *)(packet->data() + sizeof(click_brn));
  return (sizeof(click_brn) + DSRProtocol::header_length(dsr_src));
}

void
DSRPeek::add_handlers()
{
  RoutingPeek::add_handlers();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RoutingPeek)
EXPORT_ELEMENT(DSRPeek)
