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
#include "falcon_routingtable.hh"
#include "falcon_successor_maintenance.hh"

CLICK_DECLS

FalconSuccessorMaintenance::FalconSuccessorMaintenance():
  _frt(NULL),
  _lookup_timer(static_lookup_timer_hook,this),
  _start(FALCON_DEFAULT_SUCCESSOR_START_TIME),
  _update_interval(FALCON_DEFAULT_SUCCESSOR_UPDATE_INTERVAL),
  _min_successor_ping(FALCON_DEFAULT_SUCCESSOR_MIN_PING),
  _rfrt(NULL),
  _opti(FALCON_OPTIMAZATION_NONE)
{
  BRNElement::init();
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
      "MINSUCCESSORPING",  cpkP, cpInteger, &_min_successor_ping,
      "DEBUG", cpkN, cpInteger, &_debug,
      "OPTIMIZATION", cpkN, cpInteger, &_opti,
      cpEnd) < 0)
    return -1;

  return 0;
}

static void notify_callback_func(void *e, int status)
{
  FalconSuccessorMaintenance *f = reinterpret_cast<FalconSuccessorMaintenance *>(e);
  f->handle_routing_update_callback(status);
}

int FalconSuccessorMaintenance::initialize(ErrorHandler *)
{
  _frt->add_update_callback(notify_callback_func,(void*)this);
  click_brn_srandom();
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( _start + click_random() % _update_interval );
  return 0;
}

void
FalconSuccessorMaintenance::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  (reinterpret_cast<FalconSuccessorMaintenance *>(f))->successor_maintenance();
  (reinterpret_cast<FalconSuccessorMaintenance *>(f))->set_lookup_timer();
}

void
FalconSuccessorMaintenance::set_lookup_timer()
{
  _lookup_timer.reschedule_after_msec( _update_interval );
}

void
FalconSuccessorMaintenance::successor_maintenance()
{
  BRN_DEBUG("Successor maintenance timer");
  BRN_DEBUG("Successor is fix: %s",String(_frt->isFixSuccessor()).c_str());
  //TODO: check age of succ and set him fix if the information is not too old
  if ( (! _frt->isFixSuccessor()) && ( _frt->_me->_status != STATUS_LEAVE ) && ( _frt->successor ) ) {
    BRN_DEBUG("%s: Check for successor: %s.", _frt->_me->_ether_addr.unparse().c_str(),
                                              _frt->successor->_ether_addr.unparse().c_str() );

    _frt->successor->set_last_ping_now();
    WritablePacket *p = DHTProtocolFalcon::new_route_request_packet(_frt->_me, _frt->successor,
                                                FALCON_MINOR_REQUEST_SUCCESSOR, FALCON_RT_POSITION_SUCCESSOR);
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


//TODO: Check whether successor change while add_node(succ). Use this as indicator for fixSuccessor
void
FalconSuccessorMaintenance::handle_reply_succ(Packet *packet, bool isUpdate)
{
  uint16_t position;

  DHTnode succ;
  DHTnode src;
  uint8_t metric;
  BRN_DEBUG("handle_reply_succ");

	if (_rfrt != NULL)
  DHTProtocolFalcon::get_info(packet, &src, &succ, &position,&metric);
else
  DHTProtocolFalcon::get_info(packet, &src, &succ, &position);
  BRN_DEBUG("ME: %s Src: %s",_frt->_me->_ether_addr.unparse().c_str(),src._ether_addr.unparse().c_str());
  if (_frt->successor != NULL)  
    BRN_DEBUG("Curr Succ: %s SucS: %s",_frt->successor->_ether_addr.unparse().c_str(),succ._ether_addr.unparse().c_str());

  if ( _min_successor_ping == _frt->get_successor_counter()){
	//when i get a reply and my successor is fix then i should check my succ from the beginning
	_frt->fixSuccessor(false); 
     // additionally stop passive monitoring when its on
	if (_frt->is_passive_monitoring()) _frt->set_passive_monitoring(false);
  }
  if ( ! isUpdate ) {  // if it's not an update, it's a normal reply so inc the counter and
                       // check if we have enough, so that succ can be set to be fix
    _frt->inc_successor_counter();
    if ( _min_successor_ping == _frt->get_successor_counter() ) {
      _frt->fixSuccessor(true);              // i check the successor so now its fix (for now)
      _frt->_lastUpdatedPosition=0;
    }
  }

  _frt->add_node(&src);
  _frt->add_node(&succ);

  /* Update HAWK RoutingTable used for dht-based routing */
  if ( (_rfrt != NULL) /*&& (_opti != FALCON_OPTIMAZATION_FWD_TO_BETTER_SUCC ) && (_opti != FALCON_OPT_FWD_SUCC_WITH_SUCC_HINT)*/) {
    if ( memcmp(succ._ether_addr.data(), src._ether_addr.data(),6) == 0 ) {
      BRN_INFO("Add neighbourhop.");
    } else {
      BRN_INFO("Add foreign hop");
    }

    //click_ether *annotated_ether = reinterpret_cast<click_ether *>(packet->ether_header());
    //EtherAddress srcEther = EtherAddress(annotated_ether->ether_shost);

    BRN_DEBUG("Dest %s Next Phy Hop: %s",succ._ether_addr.unparse().c_str(), src._ether_addr.unparse().c_str());

    _rfrt->addEntry(&(succ._ether_addr), succ._md5_digest, succ._digest_length,
                   /*&srcEther*/ NULL, &(src._ether_addr),metric + (_rfrt->getEntry(&(src._ether_addr)))->_metric);
    
  }
}

//TODO: think about forward and backward-search for successor. Node should switch from back- to forward if it looks faster

void
FalconSuccessorMaintenance::handle_request_succ(Packet *packet)
{
  uint16_t position;

  DHTnode succ;
  DHTnode src;

  BRN_DEBUG("handle_request_succ");

  DHTProtocolFalcon::get_info(packet, &src, &succ, &position);

  /** Hawk-Routing stuff. TODO: this should move to extra funtion */
  if ( _rfrt != NULL ) {
    const click_ether *annotated_ether = reinterpret_cast<const click_ether *>(packet->ether_header());
    if ( memcmp(annotated_ether->ether_shost, src._ether_addr.data(),6) == 0 ) {
      BRN_INFO("Add neighbourhop.");
    } else {
      BRN_INFO("Add foreign hop");
    }

    //EtherAddress srcEther = EtherAddress(annotated_ether->ether_shost);

  // _rfrt->addEntry(&(src._ether_addr), src._md5_digest, src._digest_length,
  //                  &(srcEther));
  }
  /** End Hawk stuff */

  if ((_opti == FALCON_OPT_SUCC_HINT) || (_opti == FALCON_OPT_FWD_SUCC_WITH_SUCC_HINT)) {
    DHTnode* old_pre = NULL;
    if(_frt->predecessor != NULL) old_pre = new DHTnode(_frt->predecessor->_ether_addr);

    _frt->add_node(&src);
    //if predecessor changed tell it old predecessor
    if (old_pre != NULL && memcmp(old_pre->_ether_addr.data(),_frt->predecessor->_ether_addr.data(),6) != 0 ) {

      BRN_DEBUG("i got new predecessor: %s, tell ist old one: %s",_frt->predecessor->_ether_addr.unparse().c_str(), old_pre->_ether_addr.unparse().c_str());
      //with fwd optimization send a succ request instead of the old predecesosr to be faster
      if (_opti ==  FALCON_OPT_FWD_SUCC_WITH_SUCC_HINT) {


		// from here i know a direct route to the new pred so i send a packet to old one therefore he gets the rest of the route
		WritablePacket *p = DHTProtocolFalcon::new_route_reply_packet(_frt->predecessor, old_pre, FALCON_MINOR_UPDATE_SUCCESSOR,
                                                                  _frt->predecessor, FALCON_RT_POSITION_SUCCESSOR,
									   (_rfrt->getEntry(&((_frt->predecessor)->_ether_addr)))->_metric, NULL);
	//WritablePacket *p = DHTProtocolFalcon::new_route_request_packet(old_pre, _frt->predecessor,
        //                                                                FALCON_MINOR_REQUEST_SUCCESSOR, FALCON_RT_POSITION_SUCCESSOR);
	//fwd it, to have the metric to old pre
	//p->pull(sizeof(struct click_brn) + sizeof(click_ether));
	//if(_rfrt != NULL)
	// 	p = DHTProtocolFalcon::fwd_route_request_packet(old_pre, _frt->predecessor,old_pre,(_rfrt->getEntry(&(old_pre->_ether_addr)))->_metric, p);  //recyl. packet
	//else 
	// p = DHTProtocolFalcon::fwd_route_request_packet(old_pre, _frt->predecessor,old_pre, p);  //recyl. packet
      BRN_DEBUG("send packet with metric %d",(_rfrt->getEntry(&(old_pre->_ether_addr)))->_metric);
	output(0).push(p);

      } else {

        Packet* dummy = NULL;
	 WritablePacket *q = NULL;
	//fill dht-header with metric to succ
	 if (_rfrt != NULL)
		 q = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, old_pre,  FALCON_MINOR_REPLY_SUCCESSOR,
                                                                      _frt->predecessor, FALCON_RT_POSITION_SUCCESSOR,
										(_rfrt->getEntry(&(_frt->predecessor->_ether_addr)))->_metric,dummy);
	else
        q = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, old_pre,  FALCON_MINOR_REPLY_SUCCESSOR,
                                                                      _frt->predecessor, FALCON_RT_POSITION_SUCCESSOR,dummy);
        output(0).push(q);

      }

    }

    if ( old_pre != NULL ) {
      delete old_pre;
      old_pre = NULL;
    }
  } else _frt->add_node(&src);

  if ( succ.equals(_frt->_me) ) {                  //request really for me ??
    //Wenn er mich fuer seinen Nachfolger haelt, teste ob er mein Vorgaenger ist oder mein Vorgaenger
    //fuer ihn ein besserer Nachfolger ist.
    if ( src.equals(_frt->predecessor) ) {        //src is my pre, so everything is good
      BRN_DEBUG("I'm his successor !");           //just send reply
      BRN_DEBUG("Src: %s Dst: %s Node: %s", src._ether_addr.unparse().c_str(), _frt->_me->_ether_addr.unparse().c_str(),
                                            succ._ether_addr.unparse().c_str());
WritablePacket *p = NULL;
	 if (_rfrt != NULL)
		 p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_REPLY_SUCCESSOR,
                                                                    _frt->_me, FALCON_RT_POSITION_SUCCESSOR,
										(_rfrt->getEntry(&(_frt->predecessor->_ether_addr)))->_metric,packet);
        else
      p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_REPLY_SUCCESSOR,
                                                                    _frt->_me, FALCON_RT_POSITION_SUCCESSOR, packet);
      output(0).push(p);
    } else {                                     //he is NOT my pre, i try to give a better one
      BRN_DEBUG("He (%s) is before my predecessor: %s",
                src._ether_addr.unparse().c_str(),_frt->predecessor->_ether_addr.unparse().c_str() );
                                                //don't search for too old information

      DHTnode *best_succ;

      if ( _rfrt != NULL ) {
        best_succ = _frt->findBestSuccessor(&src, 20/* max age 20 s*/, &(_rfrt->_known_hosts)); //TODO: use params
      } else {
        best_succ = _frt->findBestSuccessor(&src, 20/* max age 20 s*/); //TODO: use params
      }

      if ( best_succ->equals(_frt->_me) ) best_succ = _frt->predecessor; //if predecessor is too old, then it's possible
                                                                         //that findBestSuccessor returns me
                                                                         //and that wrong at this point. so set best to
                                                                         //pred manually. TODO: do it better
      /* Now just some info to print to find errors. TODO; Maybe remove in the future.*/
      if ( best_succ->equals(_frt->predecessor) ) {
        BRN_DEBUG("------1-------: My pre is his succ");
      } else {
        BRN_DEBUG("------2-------: I've better succ than my pre.  ME: %s  Src: %s",
                            _frt->_me->_ether_addr.unparse().c_str(), src._ether_addr.unparse().c_str());
        BRN_DEBUG("------2-------: I've better succ than my pre.  Succ: %s  Pre: %s",
                         best_succ->_ether_addr.unparse().c_str(), _frt->predecessor->_ether_addr.unparse().c_str() );
      }
      /*end of debug stuff*/
/*	BRN_DEBUG("finger.size:%d",_frt->_fingertable.size());
	 for( int i = 0; i < _frt->_fingertable.size(); i++ )
   	 {
    		DHTnode* fnode = _frt->_fingertable.get_dhtnode(i);
	       BRN_DEBUG(" %d: %s",i,fnode->_ether_addr.unparse().c_str());
   	 }*/


      //if you want optimization and you are not using hawk for routing then fwd the packet
      WritablePacket *p;
      if (/* (_rfrt == NULL) && */ (_opti == FALCON_OPTIMAZATION_FWD_TO_BETTER_SUCC ) || (_opti == FALCON_OPT_FWD_SUCC_WITH_SUCC_HINT) ) {
        BRN_INFO("Fwd request");
	if (_rfrt != NULL)
	 p = DHTProtocolFalcon::fwd_route_request_packet(&src, best_succ, &src,(_rfrt->getEntry(&(src._ether_addr)))->_metric, packet);  //recyl. packet
	else
        p = DHTProtocolFalcon::fwd_route_request_packet(&src, best_succ, _frt->_me, packet);  //recyl. packet
      } else {  //otherwise send a reply with the better information
        BRN_INFO("Send reply");
	 if ( _rfrt != NULL)
          p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src,  FALCON_MINOR_REPLY_SUCCESSOR,
                                                      best_succ, FALCON_RT_POSITION_SUCCESSOR,(_rfrt->getEntry(&(best_succ->_ether_addr)))->_metric,packet);
	 else
	   p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src,  FALCON_MINOR_REPLY_SUCCESSOR,
                                                      best_succ, FALCON_RT_POSITION_SUCCESSOR,packet);
      }
      output(0).push(p);
    }
  } else {
    BRN_WARN("Error??? Me: %s Succ: %s", _frt->_me->_ether_addr.unparse().c_str(), succ._ether_addr.unparse().c_str());
    packet->kill();
  }
}

/*************************************************************************************************/
/******************************** C A L L B A C K ************************************************/
/*************************************************************************************************/

void
FalconSuccessorMaintenance::handle_routing_update_callback(int status)
{
  if ( status == RT_UPDATE_SUCCESSOR ) {
    BRN_DEBUG("Update Successor");
  }
}


CLICK_ENDDECLS
EXPORT_ELEMENT(FalconSuccessorMaintenance)
