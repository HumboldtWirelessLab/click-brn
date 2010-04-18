#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/standard/md5.h"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable.hh"
#include "falcon_routing_peek.hh"

CLICK_DECLS

FalconRoutingPeek::FalconRoutingPeek()
{
  BRNElement::init();
}

FalconRoutingPeek::~FalconRoutingPeek()
{
}

int FalconRoutingPeek::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "ROUTINGPEEK", cpkP+cpkM, cpElement, &_routing_peek,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

static bool routing_peek_func(void */*e*/, Packet */*p*/, int /*brn_port*/)
{
  return true;
}

int FalconRoutingPeek::initialize(ErrorHandler *)
{
  _routing_peek->add_routing_peek(routing_peek_func, (void*)this, BRN_PORT_FALCON);
  return 0;
}

void
FalconRoutingPeek::add_handlers()
{
  BRNElement::add_handlers();
}


CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingPeek)
