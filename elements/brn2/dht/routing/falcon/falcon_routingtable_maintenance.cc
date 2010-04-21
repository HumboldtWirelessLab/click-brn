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
#include "falcon_functions.hh"
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
  click_srandom(_frt->_me->_ether_addr.hashcode());
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
  if ( _frt->isFixSuccessor() && ( _frt->_me->_status != STATUS_LEAVE  ) ) {
    DHTnode *nextToAsk = _frt->_fingertable.get_dhtnode(_frt->_lastUpdatedPosition);

    //TODO: this is a workaround for an error in the maintenance of th elastUpdatedPosition. Please solve the problem.
    if ( nextToAsk == NULL ) {
      BRN_ERROR("No node left to ask. Reset update Position. Position: %d FT-size: %d",_frt->_lastUpdatedPosition, _frt->_fingertable.size());
      BRN_ERROR("Table: \n%s",_frt->routing_info().c_str());
      assert(nextToAsk != NULL );
      return;
    }

    //TODO: There was an error, while setup the Routing-Table. I fixed it, but if there is an error again please save this output (Routing Table)
    if ( _frt->_me->equals(nextToAsk) ) {
      BRN_ERROR("Src-Node should not be Dst-Node ! Error in Routing-Table !");
      BRN_ERROR("Table: \n%s",_frt->routing_info().c_str());
      assert(! _frt->_me->equals(nextToAsk));
    }

    nextToAsk->set_last_ping_now();
    WritablePacket *p = DHTProtocolFalcon::new_route_request_packet(_frt->_me, nextToAsk, FALCON_MINOR_REQUEST_POSITION, _frt->_lastUpdatedPosition);
    output(0).push(p);
  }
}

void
FalconRoutingTableMaintenance::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    switch ( DHTProtocol::get_type(packet) ) {
      case FALCON_MINOR_REQUEST_POSITION:
        handle_request_pos(packet);
        packet->kill();
        break;
      case FALCON_MINOR_REPLY_POSITION:
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
FalconRoutingTableMaintenance::handle_request_pos(Packet *packet)
{
  uint16_t position;

  DHTnode src;
  DHTnode node;
  DHTnode *posnode;
  DHTnode *find_node;

  BRN_DEBUG("handle_request_pos");

  DHTProtocolFalcon::get_info(packet, &src, &node, &position);

  _frt->add_node(&src);

  find_node = _frt->find_node(&src);
  if ( find_node) _frt->set_node_in_reverse_FT(find_node, position);
  else BRN_ERROR("Couldn't find inserted node");

  /** Check for and handle error in Routingtable of the source of the request. If he think that i'm his succ
      but he is not my Pred. then send him a notice, that he is wrong and what his succ in my opinion */
  if ( ( position == 0 ) && ( ! src.equals(_frt->predecessor) ) ) {
    BRN_WARN("Node (%s) ask for my position 0 (for him i'm his successor) but is not my predecessor",
                                                                   src._ether_addr.unparse().c_str());
    BRN_WARN("me: %s, mypre: %s , node. %s", _frt->_me->_ether_addr.unparse().c_str(),
                                     _frt->predecessor->_ether_addr.unparse().c_str(), src._ether_addr.unparse().c_str());

    DHTnode *best_succ = _frt->findBestSuccessor(&src,20/* max age 20 s*/);

    if ( best_succ->equals(_frt->predecessor) ) {
      BRN_DEBUG("RoutingTable :------1-------: My pre is his succ");
    } else {
      BRN_DEBUG("RoutingTable :------2-------: I've better succ than my pre. ME: %s  Src: %s  Succ: %s Pre: %s",_frt->_me->_ether_addr.unparse().c_str(), src._ether_addr.unparse().c_str(), best_succ->_ether_addr.unparse().c_str(), _frt->predecessor->_ether_addr.unparse().c_str() );
      BRN_DEBUG("%s",_frt->routing_info().c_str());
    }

    WritablePacket *p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_UPDATE_SUCCESSOR,
                                                                  best_succ, FALCON_RT_POSITION_SUCCESSOR);
    output(0).push(p);
    return;
  }

  /** search for the entry he wants to know */
  posnode = _frt->_fingertable.get_dhtnode(position);

  if ( posnode != NULL ) {
    BRN_DEBUG("Node: %s ask me (%s) for pos: %d . Ans: %s",src._ether_addr.unparse().c_str(),
                               _frt->_me->_ether_addr.unparse().c_str(), position, posnode->_ether_addr.unparse().c_str());
    WritablePacket *p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_REPLY_POSITION,
                                                                              posnode, position);
    output(0).push(p);
  } else {
    BRN_WARN("HE wants to know a node that i didn't know. Send negative reply.");
    node._status = STATUS_NONEXISTENT; //i don't have such node
    node.set_nodeid(NULL, 0);          //its and invalid node
    WritablePacket *p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_REPLY_POSITION,
                                                                             &node, position);
    output(0).push(p);
  }
}

void
FalconRoutingTableMaintenance::handle_reply_pos(Packet *packet)
{
  uint16_t position;

  DHTnode node, src;
  DHTnode *nc, *preposnode;

  BRN_DEBUG("handle_reply_pos");

  DHTProtocolFalcon::get_info(packet, &src, &node, &position);

  _frt->add_node(&src);

  if ( node._status == STATUS_NONEXISTENT ) {
    BRN_WARN("Node %s has no node in table on position %d.",src._ether_addr.unparse().c_str(),position);
    _frt->setLastUpdatedPosition(0);
    return;
  }

  //TODO: update node

  BRN_DEBUG("I (%s) ask Node (%s) for pos: %d . Ans: %s",_frt->_me->_ether_addr.unparse().c_str(),
                                           src._ether_addr.unparse().c_str(), position, node._ether_addr.unparse().c_str());

  preposnode = _frt->_fingertable.get_dhtnode(position);

  nc = _frt->find_node(&node);            //Find node
  if ( nc == NULL ) {                     //if not included (it's new) TODO: use _frt->add_node(node) in the beginning
    _frt->add_node(&node);                //then add
    nc = _frt->find_node(&node);          //and get
  }

  /**
   * Die Abfrage stellt sicher, dass man nicht schon einmal im Kreis rum ist, und der Knoten den man grad befragt hat einen
   * Knoten zuückliegt der schon wieder vor mir liegt. Natuerlich darf es der Knoten selbst auch nicht sein
  */
  // make sure that the node n on position p+1 does not stand between this node and node on position p
  //TODO: make sure that we have to check, that node on new posotion p+1 is not the node on position p
  if ( ! ( FalconFunctions::is_in_between( _frt->_me, preposnode, &node) || _frt->_me->equals(&node) ||
                                                                           preposnode->equals(&node) ) ) {
    _frt->add_node_in_FT(nc, position + 1); //add node to Fingertable. THis also handles, that the node is already in
                                            //the Fingertable, but on another position
    if ( nc->equals(_frt->predecessor) )    //in some cases the last node in the Fingertable is the predecessor.
      _frt->setLastUpdatedPosition(0);      //then we can update our successor (position 0) next.
    else
      _frt->incLastUpdatedPosition();       //TODO: add this in add_node_in_FT (??). is that better ??

  } else {                             //---> Node is backlog
    _frt->backlog = nc;
    _frt->setLastUpdatedPosition(0);
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingTableMaintenance)
