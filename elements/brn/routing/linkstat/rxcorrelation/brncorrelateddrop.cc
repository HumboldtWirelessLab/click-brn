#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "brncorrelateddrop.hh"

CLICK_DECLS

BRNCorrelatedDrop::BRNCorrelatedDrop()
  :_dropped_source(),
   _drop_pattern(0),
   _packet(0),
   _skew(0)
{
}

BRNCorrelatedDrop::~BRNCorrelatedDrop()
{
}

int
BRNCorrelatedDrop::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "PATTERN", cpkP+cpkM, cpUnsigned, &_drop_pattern,
      "SKEW", cpkP+cpkM, cpInteger, &_skew,
      "ETHERADDRESS", cpkP, cpEtherAddress, &_dropped_source,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
BRNCorrelatedDrop::simple_action(Packet *p)
{
  click_ether *ether = (click_ether *) p->data();
  EtherAddress src = EtherAddress(ether->ether_shost);
  //EtherAddress src = BRNPacketAnno::src_ether_anno(p);

  //if we only want to drop packets from a given source (_dropped_source.is_set()) than we just drop packets from src
  if ((_dropped_source.is_set()) && (_dropped_source != src)) return p;

  if ( _drop_pattern != 0 ) {
    if ( ((_drop_pattern >> ((_skew + (_packet++))%32)) & 1) == 1 ) {
      p-> kill();
      return NULL;
    }
  }

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNCorrelatedDrop)
