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

#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable.hh"
#include "falcon_successor_maintenance.hh"

CLICK_DECLS

FalconSuccessorMaintenance::FalconSuccessorMaintenance():
  _lookup_timer(static_lookup_timer_hook,this),
  _start(FALCON_DEFAULT_SUCCESSOR_START_TIME),
  _update_interval(FALCON_DEFAULT_SUCCESSOR_UPDATE_INTERVAL),
  _debug(BrnLogger::DEFAULT)
{
}

FalconSuccessorMaintenance::~FalconSuccessorMaintenance()
{
}

int FalconSuccessorMaintenance::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "STARTTIME", cpkP+cpkM, cpInteger, &_start,
      "UPDATEINT", cpkP, cpInteger, &_update_interval,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

static void notify_callback_func(void *e, int status)
{
  FalconSuccessorMaintenance *f = (FalconSuccessorMaintenance *)e;
  f->handle_routing_update_callback(status);
}

int FalconSuccessorMaintenance::initialize(ErrorHandler *)
{
  _frt->add_update_callback(notify_callback_func,(void*)this);
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( _start + click_random() % _update_interval );
  return 0;
}

void
FalconSuccessorMaintenance::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((FalconSuccessorMaintenance*)f)->successor_maintenance();
  ((FalconSuccessorMaintenance*)f)->set_lookup_timer();
}

void
FalconSuccessorMaintenance::set_lookup_timer()
{
  _lookup_timer.schedule_after_msec( _update_interval );
}

void
FalconSuccessorMaintenance::successor_maintenance()
{
  if ( ! _frt->isFixSuccessor() ) {
    BRN_DEBUG("%s: Check for successor: %s.", _frt->_me->_ether_addr.unparse().c_str(), _frt->successor->_ether_addr.unparse().c_str() );

    _frt->successor->set_last_ping_now();
    WritablePacket *p = DHTProtocolFalcon::new_route_request_packet(_frt->_me, _frt->successor, FALCON_MINOR_REQUEST_SUCCESSOR, FALCON_RT_POSITION_SUCCESSOR);
    output(0).push(p);
  }
}

void
FalconSuccessorMaintenance::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    switch ( DHTProtocol::get_type(packet) ) {
      case FALCON_MINOR_REQUEST_SUCCESSOR:
        handle_request_succ(packet);
        break;
      case FALCON_MINOR_REPLY_SUCCESSOR:
        handle_reply_succ(packet, false);
        packet->kill();
        break;
      case FALCON_MINOR_UPDATE_SUCCESSOR:
        handle_reply_succ(packet, true);
        packet->kill();
        break;
      default:
        BRN_WARN("Unknown Operation in falcon-routing");
        packet->kill();
        break;
    }
  }
}

void
FalconSuccessorMaintenance::handle_reply_succ(Packet *packet, bool isUpdate)
{
  uint8_t status;
  uint16_t position;

  DHTnode succ;
  DHTnode src;

  BRN_DEBUG("handle_reply_succ");

  DHTProtocolFalcon::get_info(packet, &src, &succ, &status, &position);

  _frt->add_node(&src);
  _frt->add_node(&succ);

  if ( isUpdate ) _frt->fixSuccessor(false);  //TODO: for now an update is only a hint which has to be proofed
  else _frt->fixSuccessor(true);

  _frt->_lastUpdatedPosition=0;
}

//TODO: think about forward and backward-seach for successor. Node should switch from back- to forward if it looks faster

void
FalconSuccessorMaintenance::handle_request_succ(Packet *packet)
{
  uint8_t status;
  uint16_t position;

  DHTnode succ;
  DHTnode src;

  BRN_DEBUG("handle_request_succ");

  DHTProtocolFalcon::get_info(packet, &src, &succ, &status, &position);

  _frt->add_node(&src);

  if ( succ.equals(_frt->_me) ) {
    //Wenn ich er mich für seinen Nachfolger hält, teste ob er mein Vorgänger ist oder mein Vorgänger für ihn ein besserer Nachfolger ist.
    if ( src.equals(_frt->predecessor) ) {
      BRN_DEBUG("I'm his successor !");
      BRN_DEBUG("Src: %s Dst: %s Node: %s",_frt->_me->_ether_addr.unparse().c_str(),src._ether_addr.unparse().c_str(),_frt->_me->_ether_addr.unparse().c_str());
      WritablePacket *p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_REPLY_SUCCESSOR, _frt->_me, FALCON_RT_POSITION_SUCCESSOR);
      packet->kill();

      output(0).push(p);
    } else {
      BRN_DEBUG("He (%s) is before my predecessor: %s",src._ether_addr.unparse().c_str(),_frt->predecessor->_ether_addr.unparse().c_str() );
      WritablePacket *p = DHTProtocolFalcon::fwd_route_request_packet(&src, _frt->predecessor, _frt->_me, packet);
      output(0).push(p);
    }
  } else {
    BRN_WARN("error??? Me: %s Succ: %s",_frt->_me->_ether_addr.unparse().c_str(),succ._ether_addr.unparse().c_str());
  }
}

/*************************************************************************************************/
/******************************** C A L L B A C K ************************************************/
/*************************************************************************************************/

void
FalconSuccessorMaintenance::handle_routing_update_callback(int status)
{
/*  if ( status == RT_UPDATE_PREDECESSOR )
    click_chatter("Update Successor");*/
}


CLICK_ENDDECLS
EXPORT_ELEMENT(FalconSuccessorMaintenance)
