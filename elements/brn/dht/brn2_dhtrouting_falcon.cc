#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "brn2_dhtrouting_falcon.hh"

#include "elements/brn/routing/nblist.hh"
#include "elements/brn/routing/linkstat/brnlinkstat.hh"

CLICK_DECLS

DHTRoutingFalcon::DHTRoutingFalcon():
  _linkstat(NULL)
{
}

DHTRoutingFalcon::~DHTRoutingFalcon()
{
}

void *DHTRoutingFalcon::cast(const char *name)
{
  if (strcmp(name, "DHTRoutingFalcon") == 0)
    return (DHTRoutingFalcon *) this;
  else if (strcmp(name, "DHTRouting") == 0)
         return (DHTRouting *) this;
       else
         return NULL;
}

int DHTRoutingFalcon::configure(Vector<String> &conf, ErrorHandler *errh)
{
  return 0;
}

int DHTRoutingFalcon::initialize(ErrorHandler *)
{
  return 0;
}

void DHTRoutingFalcon::push( int port, Packet *packet )
{
}

void DHTRoutingFalcon::add_handlers()
{
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTRoutingFalcon)
