#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "brncorrelateddrop.hh"

CLICK_DECLS

BRNCorrelatedDrop::BRNCorrelatedDrop()
  :_drop_pattern(0),
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
      "PATTERN", cpkP+cpkM, cpInteger, &_drop_pattern,
      "SKEW", cpkP+cpkM, cpInteger, &_skew,
//      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
BRNCorrelatedDrop::simple_action(Packet *p)
{
  _packet++;

  if ( _drop_pattern != 0 ) {
    if ( ( _skew + _packet ) % _drop_pattern == 0 ) {
      p-> kill();
      return NULL;
    }
  }

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNCorrelatedDrop)
