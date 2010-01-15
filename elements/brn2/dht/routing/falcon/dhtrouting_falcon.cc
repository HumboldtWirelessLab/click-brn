#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "dhtrouting_falcon.hh"
#include "falcon_routingtable.hh"

CLICK_DECLS

DHTRoutingFalcon::DHTRoutingFalcon():
  _frt(NULL)
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
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      cpEnd) < 0)
    return -1;

  return 0;
}

int DHTRoutingFalcon::initialize(ErrorHandler *)
{
  return 0;
}

DHTnode *
DHTRoutingFalcon::get_responsibly_node(md5_byte_t */*key*/)
{
  return NULL;
}

void DHTRoutingFalcon::add_handlers()
{
//  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTRoutingFalcon)
