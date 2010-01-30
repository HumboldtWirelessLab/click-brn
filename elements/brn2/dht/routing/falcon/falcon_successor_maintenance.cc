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

int FalconSuccessorMaintenance::initialize(ErrorHandler *)
{
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
    BRN_DEBUG("%s: Check for successor", _frt->_me->_ether_addr.unparse().c_str());

    WritablePacket *p = DHTProtocolFalcon::new_route_request_packet(_frt->_me, _frt->successor, FALCON_OPERATION_REQUEST_SUCCESSOR, FALCON_RT_POSITION_SUCCESSOR);
    output(0).push(p);
  }
}

void
FalconSuccessorMaintenance::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    switch ( DHTProtocolFalcon::get_operation(packet) ) {
      case FALCON_OPERATION_REQUEST_SUCCESSOR:
        if ( DHTProtocol::get_type(packet) == ROUTETABLE_REQUEST )
          handle_request_succ(packet);
        else {
          handle_reply_succ(packet, false);
          packet->kill();
        }
        break;
      case FALCON_OPERATION_UPDATE_SUCCESSOR:
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
  uint8_t operation, status, position;

  DHTnode succ;
  DHTnode src;

  BRN_DEBUG("handle_reply_succ");

  DHTProtocolFalcon::get_info(packet, &src, &succ, &operation, &status, &position);

  _frt->add_node(&succ);

  if ( isUpdate ) _frt->fixSuccessor(false);  //TODO: for now an update is only a hint which has to be proofed
  else _frt->fixSuccessor(true);

  _frt->_lastUpdatedPosition=0;
}

//TODO: think about forward and backward-seach for successor. Node shoul switch from back- to forward if it looks faster

void
FalconSuccessorMaintenance::handle_request_succ(Packet *packet)
{
  uint8_t operation, status, position;

  DHTnode succ;
  DHTnode src;

  BRN_DEBUG("handle_request_succ");

  DHTProtocolFalcon::get_info(packet, &src, &succ, &operation, &status, &position);

  _frt->add_node(&src);

  if ( succ.equals(_frt->_me) ) {
    //Wenn ich er mich für seinen Nachfolger hält, teste ob er mein Vorgänger ist oder mein Vorgänger für ihn ein besserer Nachfolger ist.
    if ( src.equals(_frt->predecessor) ) {
      BRN_DEBUG("I'm his successor !");
      WritablePacket *p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_OPERATION_REQUEST_SUCCESSOR, _frt->_me, FALCON_RT_POSITION_SUCCESSOR);
      packet->kill();

      output(0).push(p);
    } else {
      BRN_DEBUG("He is before my predecessor");
      WritablePacket *p = DHTProtocolFalcon::fwd_route_request_packet(&src, _frt->predecessor, _frt->_me, packet);
      output(0).push(p);
    }
  } else {
    BRN_WARN("error??? Me: %s Succ: %s",_frt->_me->_ether_addr.unparse().c_str(),succ._ether_addr.unparse().c_str());
  }

}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconSuccessorMaintenance)
