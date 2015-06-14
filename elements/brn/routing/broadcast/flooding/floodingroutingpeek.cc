#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <clicknet/ether.h>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "flooding.hh"
#include "floodingroutingpeek.hh"

CLICK_DECLS

FloodingRoutingPeek::FloodingRoutingPeek()
{
  RoutingPeek::init();
}

FloodingRoutingPeek::~FloodingRoutingPeek()
{
}

uint32_t
FloodingRoutingPeek::get_all_header_len(Packet *p)
{
  uint32_t header_size = 0;
  struct click_brn_bcast *bcast_header = (struct click_brn_bcast*)&(p->data()[sizeof(click_brn)]);
  uint8_t extra_size = bcast_header->extra_data_size;

  header_size += sizeof(click_brn) + sizeof(struct click_brn_bcast) + extra_size + 6;
  uint16_t *subdata = (uint16_t*)&(p->data()[header_size]);
  uint16_t subtype = ntohs(subdata[0]);

  if ( subtype == ETHERTYPE_BRN ) {
    BRN_DEBUG("Detect BrnPacket");
    click_brn *subbrn = (click_brn*)&(subdata[1]); //get brn_header
    if ( subbrn->dst_port == BRN_PORT_BCASTROUTING ) {
      BRN_DEBUG("Subbrn is bcastrouting");
      header_size += sizeof(click_brn);
    } else {
      header_size -= 12;
    }
  } else {
    BRN_DEBUG("No BRN");
  }

  BRN_DEBUG("Headersize: %d", header_size);

  return header_size;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RoutingPeek)
EXPORT_ELEMENT(FloodingRoutingPeek)
