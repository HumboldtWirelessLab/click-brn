#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable.hh"
#include "falcon_passive_monitoring.hh"

CLICK_DECLS

FalconPassiveMonitoring::FalconPassiveMonitoring():
  _lookup_timer(static_lookup_timer_hook,this)
{
  BRNElement::init();
}

FalconPassiveMonitoring::~FalconPassiveMonitoring()
{
}

int FalconPassiveMonitoring::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int FalconPassiveMonitoring::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  return 0;
}

void
FalconPassiveMonitoring::static_lookup_timer_hook(Timer *t, void */*f*/)
{
  if ( t == NULL ) click_chatter("Time is NULL");
}

void
FalconPassiveMonitoring::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    switch ( DHTProtocol::get_type(packet) ) {
      case FALCON_MINOR_PASSIVE_MONITORING_ACTIVATE:
        handle_leave_monitoring_activate(packet);
        packet->kill();
        break;
      case FALCON_MINOR_PASSIVE_MONITORING_DEACTIVATE:
        handle_leave_monitoring_deactivate(packet);
        packet->kill();
        break;
      default:
        BRN_WARN("Unknown Operation in falcon-leave");
        packet->kill();
        break;
    }
  }
}

void
FalconPassiveMonitoring::handle_leave_monitoring_activate(Packet */*packet*/)
{
}

void
FalconPassiveMonitoring::handle_leave_monitoring_deactivate(Packet */*packet*/)
{
}


CLICK_ENDDECLS
EXPORT_ELEMENT(FalconPassiveMonitoring)
