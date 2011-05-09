#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "storetunnelendpoint.hh"

CLICK_DECLS

StoreTunnelEndpoint::StoreTunnelEndpoint()
  : _reverse_arp_table()
{
  BRNElement::init();
}

StoreTunnelEndpoint::~StoreTunnelEndpoint()
{
}

int
StoreTunnelEndpoint::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "REVERSEARPTABLE", cpkP+cpkM, cpElement, &_reverse_arp_table,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}

int
StoreTunnelEndpoint::initialize(ErrorHandler *)
{
  return 0;
}

/* learns and stores ip/ethernet address combinations */
Packet *
StoreTunnelEndpoint::simple_action(Packet *p_in)
{
  BRN_DEBUG("simple_action()");

  const click_ether *ether = (click_ether *)p_in->data();
  EtherAddress src_ether_addr(ether->ether_shost);

  const click_ip *ip = p_in->ip_header();
  IPAddress src_ip_addr(ip->ip_src);

  BRN_DEBUG("* new mapping: %s -> %s", src_ether_addr.unparse().c_str(), src_ip_addr.unparse().c_str());
  _reverse_arp_table->insert(src_ether_addr, src_ip_addr );

  return p_in;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
StoreTunnelEndpoint::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StoreTunnelEndpoint)
