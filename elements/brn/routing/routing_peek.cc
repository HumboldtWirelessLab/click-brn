#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "routing_peek.hh"

CLICK_DECLS

RoutingPeek::RoutingPeek()
{
}

RoutingPeek::~RoutingPeek()
{
}

void
RoutingPeek::init()
{
  BRNElement::init();
}

int
RoutingPeek::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
RoutingPeek::initialize(ErrorHandler *)
{
  return 0;
}

void
RoutingPeek::push( int port, Packet *packet )
{
  //EtherAddress link_src;
  //EtherAddress link_dst;
  //EtherAddress src;
  //EtherAddress dst;
  //uint16_t ethertype;
    
  uint32_t header_len = get_all_header_len(packet);
    
  //get_addresses(&link_src, &link_dst, &src, &dst, &ethertype);

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
  output(port).push(packet);
}

static String
read_peek_info(Element *e, void */*thunk*/)
{
  RoutingPeek *rp = (RoutingPeek *)e;
  return "Number of peek: " + String(rp->_peeklist.size()) + "\n";
}

void
RoutingPeek::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("peek_info", read_peek_info, (void *) 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BRNElement)
ELEMENT_PROVIDES(RoutingPeek)
