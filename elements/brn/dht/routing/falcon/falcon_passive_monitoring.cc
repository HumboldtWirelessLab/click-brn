#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable.hh"
#include "falcon_passive_monitoring.hh"

CLICK_DECLS

FalconPassiveMonitoring::FalconPassiveMonitoring():
  _lookup_timer(static_lookup_timer_hook,this),
  _passive_monitoring_mode(FALCON_PASSIVE_MONITORING_MODE_DEACTIVATED)
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

  _lookup_timer.schedule_after_msec(5000);
  return 0;
}

void
FalconPassiveMonitoring::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");

  ((FalconPassiveMonitoring*)f)->check_monitoring();

  ((FalconPassiveMonitoring*)f)->_lookup_timer.reschedule_after_msec(5000);

}

/* genereate packet with reverse fingertable (FALCON_MINOR_PASSIVE_MONITORING_ACTIVATE) */
void
FalconPassiveMonitoring::check_monitoring()
{
  if ( _frt->is_passive_monitoring() ) {
    switch ( _passive_monitoring_mode ) {
      case FALCON_PASSIVE_MONITORING_MODE_DEACTIVATED:
        {
          // start passive monitoringthis
          BRN_DEBUG("Switch to passive mode");

          /*EtherAddress dst = EtherAddress(brn_ethernet_broadcast);
          WritablePacket *p = DHTProtocolFalcon::new_passive_monitor_active_packet(_frt->_me, &dst,
                                                                                &(_frt->_reverse_fingertable));
          */
          _passive_monitoring_mode = FALCON_PASSIVE_MONITORING_MODE_REQUESTING;

         // output(0).push(p);
	  break;
        }
      case FALCON_PASSIVE_MONITORING_MODE_REQUESTING:
        {
          //check for timeout
	   break;
        }
      case FALCON_PASSIVE_MONITORING_MODE_ACTIVATED:
        {
          //everything perfect just check
	   break;
        }
      case FALCON_PASSIVE_MONITORING_MODE_SIGNOFF:
        {
          // reset from signoff
          _passive_monitoring_mode = FALCON_PASSIVE_MONITORING_MODE_REQUESTING;
         break;
        }
      default:
        {
          click_chatter("unknown mode in passive monitor");
        }
    }
  } else {  //no passive monitoring is requested
    switch ( _passive_monitoring_mode ) { //our current mode
      case FALCON_PASSIVE_MONITORING_MODE_DEACTIVATED:
        {
          //everything perfect just check
	   break;
        }
      case FALCON_PASSIVE_MONITORING_MODE_REQUESTING:
        {
          // roll-back from request -> signoff
	  break;
        }
      case FALCON_PASSIVE_MONITORING_MODE_ACTIVATED:
        {
          // roll-back from request -> signoff
	   break;
        }
      case FALCON_PASSIVE_MONITORING_MODE_SIGNOFF:
        {
          //check for timeout
	   break;
        }
      default:
        {
        }
    }
  }
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
      case FALCON_MINOR_PASSIVE_MONITORING_NODEFAILURE:
        handle_node_failure(packet);
        packet->kill();
        break;
      case FALCON_MINOR_PASSIVE_MONITORING_NODEUPDATE:
        handle_node_update(packet);
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

void
FalconPassiveMonitoring::handle_node_failure(Packet */*packet*/)
{
}

void
FalconPassiveMonitoring::handle_node_update(Packet */*packet*/)
{
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconPassiveMonitoring)
