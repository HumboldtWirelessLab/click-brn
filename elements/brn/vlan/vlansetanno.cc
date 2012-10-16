#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "vlansetanno.hh"


CLICK_DECLS

//Deprecated
//TODO: CHeck whether this element is needed. Also check anno in BRNPacketAnno

VlanSetAnno::VlanSetAnno()
{
}

VlanSetAnno::~VlanSetAnno()
{
}

int
VlanSetAnno::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "VLANTABLE", cpkP+cpkM, cpBool, &_vlantable,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
VlanSetAnno::smaction(Packet *p)
{
  p->kill();
  return 0;
}

void
VlanSetAnno::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) {
    output(0).push(q);
  }
}

Packet *
VlanSetAnno::pull(int)
{
  if (Packet *p = input(0).pull()) {
    return smaction(p);
  } else {
    return 0;
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(VlanSetAnno)
