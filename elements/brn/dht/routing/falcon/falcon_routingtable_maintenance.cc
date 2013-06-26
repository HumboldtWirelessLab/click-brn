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
#include "elements/brn/brnprotocol/brnprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_functions.hh"
#include "falcon_routingtable.hh"
#include "falcon_routingtable_maintenance.hh"

CLICK_DECLS

FalconRoutingTableMaintenance::FalconRoutingTableMaintenance():
  _lookup_timer(static_lookup_timer_hook,this),
  _start(FALCON_DEFAULT_START_TIME),
  _update_interval(FALCON_DEFAULT_UPDATE_INTERVAL),
  _debug(BrnLogger::DEFAULT),
  _rounds_to_passive_monitoring(0),
  _opti(FALCON_OPTIMAZATION_NONE),
  _rfrt(NULL)
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
      "PMROUNDS", cpkP, cpInteger, &_rounds_to_passive_monitoring,
      "OPTIMIZATION", cpkP, cpInteger, &_opti,
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

  _current_round2pm = 0;

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
  _lookup_timer.reschedule_after_msec( _update_interval );
}

void
FalconRoutingTableMaintenance::table_maintenance()
{
  if ( _frt->isFixSuccessor() && ( _frt->_me->_status != STATUS_LEAVE  ) &&
       (!_frt->is_passive_monitoring()) ) {
    DHTnode *nextToAsk = _frt->_fingertable.get_dhtnode(_frt->_lastUpdatedPosition);

    assert(nextToAsk != NULL );              //TODO: there was an error in the maintenance of the
                                             //      lastUpdatedPosition. Please solve the problem.
    assert(! _frt->_me->equals(nextToAsk));  //TODO: There was an error, while setup the Routing-Table. I fixed it, but
                                             //      if there is an error again please save this output (Routing Table)
    BRN_DEBUG("Ask for position %d",_frt->_lastUpdatedPosition);

    nextToAsk->set_last_ping_now();
    WritablePacket *p = DHTProtocolFalcon::new_route_request_packet(_frt->_me, nextToAsk, FALCON_MINOR_REQUEST_POSITION,
                                                                    _frt->_lastUpdatedPosition);
    output(0).push(p);
  } else {
    _current_round2pm = 0; //TODO: not enough. Find better place to do it ??
  }
}

void
FalconRoutingTableMaintenance::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    switch ( DHTProtocol::get_type(packet) ) {
      case FALCON_MINOR_REQUEST_POSITION:
        handle_request_pos(packet);
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
  BRN_DEBUG("Got REQ for position %d",position);
  find_node = _frt->find_node(&src);

  //TODO: chack whether reverse node for position changes.
  if ( find_node) _frt->set_node_in_reverse_FT(find_node, position);
  else BRN_ERROR("Couldn't find inserted node");

  /** Hawk-Routing stuff. TODO: this should move to extra funtion */
  if ( _rfrt != NULL ) {
    click_ether *annotated_ether = (click_ether *)packet->ether_header();
    EtherAddress srcEther = EtherAddress(annotated_ether->ether_shost);
    if ( memcmp(annotated_ether->ether_shost, src._ether_addr.data(),6) == 0 ) {
      BRN_INFO("Is neighbourhop. Not added to table.");
    } else {
      BRN_INFO("Add foreign hop");
   //  _rfrt->addEntry(&(src._ether_addr), src._md5_digest, src._digest_length,
   //                 &(srcEther));
    }

   //  if ( memcmp(_frt->_me->_ether_addr.data(), node._ether_addr.data(), 6) != 0 )
   //   _rfrt->addEntry(&(node._ether_addr), node._md5_digest, node._digest_length,
   //                  &(srcEther),99999999);

  }
  /** End Hawk stuff */


  /** Check for and handle error in Routingtable of the source of the request. If he think that i'm his succ
      but he is not my Pred. then send him a notice, that he is wrong and what his succ in my opinion */
//TODO: check fingers for being a better succ and tell this my pre
  if ( ( position == 0 ) && ( ! src.equals(_frt->predecessor) ) ) {
    BRN_WARN("Node (%s) ask for my position 0 (for him i'm his successor) but is not my predecessor",
                                                                   src._ether_addr.unparse().c_str());
    BRN_WARN("me: %s, mypre: %s , node. %s", _frt->_me->_ether_addr.unparse().c_str(),
                                             _frt->predecessor->_ether_addr.unparse().c_str(), src._ether_addr.unparse().c_str());

    DHTnode *best_succ;

    if ( _rfrt != NULL ) {
      best_succ = _frt->findBestSuccessor(&src, 30/* max age 20 s*/, &(_rfrt->_known_hosts)); //TODO: use params
    } else {
      best_succ = _frt->findBestSuccessor(&src, 30/* max age 20 s*/); //TODO: use params
    }

    if ( best_succ->equals(_frt->predecessor) ) {
      BRN_DEBUG("RoutingTable :------1-------: My pre is his succ");
    } else {
      BRN_DEBUG("RoutingTable :------2-------: I've better succ than my pre. ME: %s  Src: %s  Succ: %s Pre: %s",
                _frt->_me->_ether_addr.unparse().c_str(), src._ether_addr.unparse().c_str(),
                best_succ->_ether_addr.unparse().c_str(), _frt->predecessor->_ether_addr.unparse().c_str() );

    }


    WritablePacket *p = NULL;

 if (_rfrt != NULL){
if (best_succ->equals(_frt->_me))
 p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_UPDATE_SUCCESSOR,
                                                                  best_succ, FALCON_RT_POSITION_SUCCESSOR,
									   0, packet);
else {

	if ((_opti == FALCON_OPTIMAZATION_FWD_TO_BETTER_SUCC ) || (_opti == FALCON_OPT_FWD_SUCC_WITH_SUCC_HINT) ) {
        	BRN_INFO("Fwd request");
	 	WritablePacket* pack = DHTProtocolFalcon::new_route_request_packet(&src, _frt->_me,
                                                FALCON_MINOR_REQUEST_SUCCESSOR, FALCON_RT_POSITION_SUCCESSOR);
			pack->pull(sizeof(struct click_brn) + sizeof(click_ether));
	 	p = DHTProtocolFalcon::fwd_route_request_packet(&src, best_succ, _frt->_me,(_rfrt->getEntry(&(src._ether_addr)))->_metric, pack);  //recyl. packet
	}
	else
 		p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_UPDATE_SUCCESSOR,
                                                                  best_succ, FALCON_RT_POSITION_SUCCESSOR,
									   (_rfrt->getEntry(&(best_succ->_ether_addr)))->_metric, packet);
}

}
else
 p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_UPDATE_SUCCESSOR,
                                                                  best_succ, FALCON_RT_POSITION_SUCCESSOR, packet);
    output(0).push(p);
    return;
  }

  /** search for the entry he wants to know */
  posnode = _frt->_fingertable.get_dhtnode(position);

  WritablePacket *p;

  if ( posnode != NULL ) {
    BRN_DEBUG("Node: %s ask me (%s) for pos: %d . Ans: %s",src._ether_addr.unparse().c_str(),
                               _frt->_me->_ether_addr.unparse().c_str(), position, posnode->_ether_addr.unparse().c_str());
	if (_rfrt != NULL)
    	 p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_REPLY_POSITION, posnode, position,
								(_rfrt->getEntry(&(posnode->_ether_addr)))->_metric, packet);	
	else
 	 p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_REPLY_POSITION, posnode, position, packet);
  } else {
    BRN_DEBUG("HE wants to know a node that i didn't know. Send negative reply.");
    node._status = STATUS_NONEXISTENT; //i don't have such node
    node.set_nodeid(NULL, 0);          //its and invalid node
    p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_REPLY_POSITION, &node, position, packet);
  }
  output(0).push(p);
}

void
FalconRoutingTableMaintenance::handle_reply_pos(Packet *packet)
{
  uint16_t position;
  uint8_t metric;
  DHTnode node, src;
  DHTnode *nc, *preposnode;

  BRN_DEBUG("handle_reply_pos");
if (_rfrt != NULL)
  DHTProtocolFalcon::get_info(packet, &src, &node, &position,&metric);
else
 DHTProtocolFalcon::get_info(packet, &src, &node, &position);
  _frt->add_node(&src);

  if ( node._status == STATUS_NONEXISTENT ) {
    BRN_DEBUG("Node %s has no node in table on position %d.",src._ether_addr.unparse().c_str(),position);
    _frt->setLastUpdatedPosition(0);
    return;
  }

  _frt->add_node(&node); //TODO: update node. Check that

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
   * Knoten zu�ckliegt der schon wieder vor mir liegt. Natuerlich darf es der Knoten selbst auch nicht sein
  */
  // make sure that the node n on position p+1 does not stand between this node and node on position p
  //TODO: make sure that we have to check, that node on new posotion p+1 is not the node on position p
  if ( ! ( FalconFunctions::is_in_between( _frt->_me, preposnode, &node) || _frt->_me->equals(&node) ||
         preposnode->equals(&node) ) ) {
    _frt->add_node_in_FT(nc, position + 1); //add node to Fingertable. THis also handles, that the node is already in
                                            //the Fingertable, but on another position
    if ( nc->equals(_frt->predecessor) ) {    //in some cases the last node in the Fingertable is the predecessor.
      _frt->setLastUpdatedPosition(0);      //then we can update our successor (position 0) next.
      if ( _rounds_to_passive_monitoring > 0 ) _current_round2pm++;
    } else {
      _frt->incLastUpdatedPosition();       //TODO: add this in add_node_in_FT (??). is that better ??
    }
  } else {                             //---> Node is backlog
    _frt->backlog = nc;
    _frt->setLastUpdatedPosition(0);
    if ( _rounds_to_passive_monitoring > 0 ) _current_round2pm++;
  }

  if ( ( _rounds_to_passive_monitoring > 0 ) &&
       ( _current_round2pm == _rounds_to_passive_monitoring ) ) {
     _current_round2pm = 0;  //reset for nextToAsk
     _frt->set_passive_monitoring(true); //enable pm, this will
                                         //be disabled by passiv monitoring (and rt ??)
  }

    /** Hawk-Routing stuff. TODO: this should move to extra funtion */
  if ( _rfrt != NULL ) {

    BRN_INFO("Add foreign hop");

    //don't add route to myself
    if ( memcmp(_frt->_me->_ether_addr.data(), node._ether_addr.data(), 6) != 0 ) {
       //TODO: whats with annos
      click_ether *annotated_ether = (click_ether *)packet->ether_header();
      EtherAddress srcEther = EtherAddress(annotated_ether->ether_shost);

      _rfrt->addEntry(&(node._ether_addr), node._md5_digest, node._digest_length,
                      NULL, &(src._ether_addr),metric + (_rfrt->getEntry(&(src._ether_addr)))->_metric);
    }
  }
  /** End Hawk stuff */

}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingTableMaintenance)
