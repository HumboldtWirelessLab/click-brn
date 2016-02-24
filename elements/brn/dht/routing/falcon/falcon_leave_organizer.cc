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
#include "falcon_leave_organizer.hh"

CLICK_DECLS

FalconLeaveOrganizer::FalconLeaveOrganizer():
  _frt(NULL),
  _lookup_timer(static_lookup_timer_hook,this),
  _max_retries(0),
  _mode(FALCON_LEAVE_MODE_IDLE),
  _new_id_len(0),
  _debug(BrnLogger::DEFAULT)
{
  memset(_new_id,0,sizeof(_new_id));
}

FalconLeaveOrganizer::~FalconLeaveOrganizer()
{
}

int FalconLeaveOrganizer::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "RETRIES", cpkP, cpInteger, &_max_retries,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int FalconLeaveOrganizer::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  return 0;
}

void
FalconLeaveOrganizer::static_lookup_timer_hook(Timer *t, void */*f*/)
{
  if ( t == NULL ) click_chatter("Time is NULL");
}

bool
FalconLeaveOrganizer::start_leave(md5_byte_t *id, int id_len )
{
  if ( _mode != FALCON_LEAVE_MODE_IDLE ) return false;
  _frt->_me->_status = STATUS_LEAVE;

  if ( id_len > 0 ) {
    memcpy(_new_id, id, MAX_NODEID_LENTGH);
    _new_id_len = MAX_NODEID_LENTGH;
  } else
    _new_id_len = 0;

  for ( int n = 0; n < _frt->_reverse_fingertable.size(); n++ ) {
    DHTnode *node = _frt->_reverse_fingertable.get_dhtnode(n);
    rev_list.add_dhtnode(node);

    WritablePacket *p = DHTProtocolFalcon::new_route_leave_packet(_frt->_me, node, FALCON_MINOR_LEAVE_NETWORK_NOTIFICATION, _frt->successor, n);

    BRN_DEBUG("Send leave");
    output(0).push(p);
  }

  if ( _frt->index_in_reverse_FT(_frt->successor) == -1 ) {
    WritablePacket *p = DHTProtocolFalcon::new_route_leave_packet(_frt->_me, _frt->successor, FALCON_MINOR_LEAVE_NETWORK_NOTIFICATION,
                                                                             _frt->predecessor, FALCON_RT_POSITION_PREDECESSOR);
    rev_list.add_dhtnode(_frt->successor);
    BRN_DEBUG("Send leave successor");     //i'm the pred of my succ so i have to tell him that i'll leave
    output(0).push(p);
  }


  return true;
}

void
FalconLeaveOrganizer::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    switch ( DHTProtocol::get_type(packet) ) {
      case FALCON_MINOR_LEAVE_NETWORK_NOTIFICATION:
        handle_leave_request(packet);
        packet->kill();
        break;
      case FALCON_MINOR_LEAVE_NETWORK_REPLY:
        handle_leave_reply(packet);
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
FalconLeaveOrganizer::handle_leave_request(Packet *packet)
{
  uint16_t position;

  DHTnode src, node;
  DHTnode *p_node;

  int index,oldindex;

  BRN_DEBUG("handle_leave_request");

  DHTProtocolFalcon::get_info(packet, &src, &node, &position);

  BRN_DEBUG("handle_leave_request: Position %d",position);

  src._status = STATUS_LEAVE;
  _frt->add_node(&node);
  _frt->add_node(&src);

  p_node = _frt->find_node(&node);

  index = _frt->index_in_FT(&node);
  oldindex = _frt->index_in_FT(&src);

  if ( oldindex != -1 ) {        //Node was in Fingertable
    if ( index != -1 )           //His successor is not in my FT so no problem
      _frt->clear_FT(index);
    _frt->set_node_in_FT(p_node, oldindex);
    if ( oldindex == 0 ) {
      _frt->fixSuccessor(false);
      _frt->successor = p_node;
    }
  }

  if ( position == FALCON_RT_POSITION_PREDECESSOR ) _frt->predecessor = p_node;

  WritablePacket *p = DHTProtocolFalcon::new_route_leave_packet(_frt->_me, &src, FALCON_MINOR_LEAVE_NETWORK_REPLY, &node, position);

  output(0).push(p);
}

void
FalconLeaveOrganizer::handle_leave_reply(Packet *packet)
{
  uint16_t position;

  DHTnode src, node;

  BRN_DEBUG("handle_leave_request");

  DHTProtocolFalcon::get_info(packet, &src, &node, &position);

  _frt->_reverse_fingertable.remove_dhtnode(&src);
  rev_list.remove_dhtnode(&src);

  BRN_DEBUG("Del node with index %d",position);

  if ( rev_list.size() == 0 ) {
    _mode = FALCON_LEAVE_MODE_IDLE;
    _frt->reset();
    if ( _new_id_len != 0 ) {
      _frt->_me->set_nodeid(_new_id);
      _frt->_me->_status = STATUS_OK;
    }
  }
}


CLICK_ENDDECLS
EXPORT_ELEMENT(FalconLeaveOrganizer)
