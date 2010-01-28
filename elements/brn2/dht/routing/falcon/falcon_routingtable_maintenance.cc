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

#include "dhtrouting_falcon.hh"
#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable.hh"
#include "falcon_routingtable_maintenance.hh"

CLICK_DECLS

FalconRoutingTableMaintenance::FalconRoutingTableMaintenance():
  _lookup_timer(static_lookup_timer_hook,this),
  _start(FALCON_DEFAULT_START_TIME),
  _update_interval(FALCON_DEFAULT_UPDATE_INTERVAL),
  _debug(BrnLogger::DEFAULT)
{
}

FalconRoutingTableMaintenance::~FalconRoutingTableMaintenance()
{
}

int FalconRoutingTableMaintenance::configure(Vector<String> &conf, ErrorHandler *errh)
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

int FalconRoutingTableMaintenance::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( _start + click_random() % _update_interval );
  return 0;
}

void
FalconRoutingTableMaintenance::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((FalconRoutingTableMaintenance*)f)->table_maintenance();
  ((FalconRoutingTableMaintenance*)f)->set_lookup_timer();
}

void
FalconRoutingTableMaintenance::set_lookup_timer()
{
  _lookup_timer.schedule_after_msec( _update_interval );
}

void
FalconRoutingTableMaintenance::table_maintenance()
{
  if ( _frt->successor != NULL ) {
    if ( ! _frt->isFixSuccessor() ) {
      BRN_DEBUG("%s: Check for successor", _frt->_me->_ether_addr.unparse().c_str());

      WritablePacket *p = DHTProtocolFalcon::new_route_request_packet(_frt->_me, _frt->successor, FALCON_OPERATION_REQUEST_SUCCESSOR, FALCON_RT_POSITION_SUCCESSOR);
      output(0).push(p);
    } else {
      DHTnode *nextToAsk = _frt->_fingertable.get_dhtnode(_frt->_lastUpdatedPosition);

      WritablePacket *p = DHTProtocolFalcon::new_route_request_packet(_frt->_me, nextToAsk, FALCON_OPERATION_REQUEST_POSITION, _frt->_lastUpdatedPosition);
      output(0).push(p);
    }
  }
}

void
FalconRoutingTableMaintenance::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    switch ( DHTProtocolFalcon::get_operation(packet) ) {
      case FALCON_OPERATION_REQUEST_SUCCESSOR:
        if ( DHTProtocol::get_type(packet) == ROUTETABLE_REQUEST )
          handle_request_succ(packet);
        else {
          handle_reply_succ(packet);
          packet->kill();
        }
        break;
      case FALCON_OPERATION_REQUEST_POSITION:
        if ( DHTProtocol::get_type(packet) == ROUTETABLE_REQUEST )
          handle_request_pos(packet);
        else
          handle_reply_pos(packet);

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
FalconRoutingTableMaintenance::handle_reply_succ(Packet *packet)
{
  uint8_t operation, status, position;

  DHTnode succ;
  DHTnode src;

  BRN_DEBUG("handle_reply_succ");

  DHTProtocolFalcon::get_info(packet, &src, &succ, &operation, &status, &position);

  _frt->add_node(&succ);
  _frt->fixSuccessor(true);
}

void
FalconRoutingTableMaintenance::handle_request_succ(Packet *packet)
{
  uint8_t operation, status, position;

  DHTnode succ;
  DHTnode src;

  BRN_DEBUG("handle_request_succ");

  DHTProtocolFalcon::get_info(packet, &src, &succ, &operation, &status, &position);

  _frt->add_node(&src);

  if ( succ.equals(_frt->_me) ) {
    //Wenn ich er mich für seinen Nachfolger hält, teste ob er mein Vorgänger ist oder mein Vorgänger für ihn ein besserer Nachfolger ist.
    if ( _frt->isInBetween(_frt->_me, _frt->predecessor, &src) ) {
      if ( src.equals(_frt->predecessor) ) {
        BRN_DEBUG("I'm his successor !");
        WritablePacket *p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_OPERATION_REQUEST_SUCCESSOR, _frt->_me, FALCON_RT_POSITION_SUCCESSOR);
        packet->kill();

        output(0).push(p);
      }
    } else {
      BRN_DEBUG("He is before my predecessor");
      WritablePacket *p = DHTProtocolFalcon::fwd_route_request_packet(&src, _frt->predecessor, _frt->_me, packet);
      output(0).push(p);
    }


  } else {
    BRN_WARN("error??? Me: %s Succ: %s",_frt->_me->_ether_addr.unparse().c_str(),succ._ether_addr.unparse().c_str());
  }
 // _frt->

}

void
FalconRoutingTableMaintenance::handle_request_pos(Packet *packet)
{
  uint8_t operation, status, position;

  DHTnode src;
  DHTnode node;
  DHTnode *posnode;

  BRN_DEBUG("handle_request_pos");

  DHTProtocolFalcon::get_info(packet, &src, &node, &operation, &status, &position);

  if ( ! node.equals(_frt->_me) ) BRN_WARN("Got packet, but he didn't ask me");

  posnode = _frt->_fingertable.get_dhtnode(position);
  if ( posnode != NULL ) {
    BRN_DEBUG("Node: %s ask me (%s) for pos: %d . Ans: %s",src._ether_addr.unparse().c_str(), _frt->_me->_ether_addr.unparse().c_str(),position, posnode->_ether_addr.unparse().c_str());
    WritablePacket *p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_OPERATION_REQUEST_POSITION, posnode, position);

    output(0).push(p);
  }
}

void
FalconRoutingTableMaintenance::handle_reply_pos(Packet *packet)
{
  uint8_t operation, status, position;

  DHTnode node;
  DHTnode src;
  DHTnode *nc;
  DHTnode *preposnode;

  BRN_DEBUG("handle_reply_pos");

  DHTProtocolFalcon::get_info(packet, &src, &node, &operation, &status, &position);

  BRN_DEBUG("I (%s) ask Node (%s) for pos: %d . Ans: %s",_frt->_me->_ether_addr.unparse().c_str(), src._ether_addr.unparse().c_str(),position, node._ether_addr.unparse().c_str());

  preposnode = _frt->_fingertable.get_dhtnode(position);

  if ( ! _frt->isInBetween(preposnode, _frt->_me, &node) ) {
    nc = _frt->find_node(&node);
    if ( nc == NULL ) {
      _frt->add_node(&node);
      nc = _frt->find_node(&node);
    }

    _frt->add_node_in_FT(nc, position + 1);

    _frt->_lastUpdatedPosition++; //TODO: add this in add_node_in_FT

  } else {                        //node is backlog
    nc = _frt->find_node(&node);
    if ( nc == NULL ) {
      _frt->add_node(&node);
      nc = _frt->find_node(&node);
    }

    _frt->backlog = nc;
    _frt->_lastUpdatedPosition = 0;
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingTableMaintenance)
