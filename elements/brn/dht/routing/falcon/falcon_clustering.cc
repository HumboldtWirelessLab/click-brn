#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable.hh"
#include "falcon_clustering.hh"

CLICK_DECLS

FalconClustering::FalconClustering():
  _lookup_timer(static_lookup_timer_hook,this)
{
  BRNElement::init();
}

FalconClustering::~FalconClustering()
{
}

int FalconClustering::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "CLUSTERING", cpkP+cpkM, cpElement, &_clustering,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int FalconClustering::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  return 0;
}

void
FalconClustering::static_lookup_timer_hook(Timer *t, void */*f*/)
{
  if ( t == NULL ) click_chatter("Time is NULL");
}

void
FalconClustering::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    switch ( DHTProtocol::get_type(packet) ) {
     default:
        BRN_WARN("Unknown Operation in falcon-leave");
        packet->kill();
        break;
    }
  }
}

void
FalconClustering::add_handlers()
{
  BRNElement::add_handlers();
}


CLICK_ENDDECLS
EXPORT_ELEMENT(FalconClustering)
