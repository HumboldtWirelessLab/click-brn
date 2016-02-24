#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/standard/brn_md5.hh"

#include "elements/brn/dht/protocol/dhtprotocol.hh"
#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"

#include "dhtprotocol_dart.hh"

#include "dart_functions.hh"
#include "dart_routingtable.hh"
#include "dart_routingtable_maintenance.hh"



CLICK_DECLS

DartRoutingTableMaintenance::DartRoutingTableMaintenance():
  _drt(NULL),
  _lookup_timer(static_lookup_timer_hook,this),
  _starttime(FALCON_DEFAULT_START_TIME),
  _activestart(false),
  _update_interval(FALCON_DEFAULT_UPDATE_INTERVAL)
{
  BRNElement::init();
}

DartRoutingTableMaintenance::~DartRoutingTableMaintenance()
{
}

int DartRoutingTableMaintenance::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DRT", cpkP+cpkM, cpElement, &_drt,
      "ACTIVESTART", cpkP+cpkM, cpBool, &_activestart,
      "STARTTIME", cpkP+cpkM, cpInteger, &_starttime,
      "UPDATEINT", cpkP, cpInteger, &_update_interval,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int DartRoutingTableMaintenance::initialize(ErrorHandler *)
{
  click_brn_srandom();

  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( _starttime + click_random() % _update_interval );
  return 0;
}

void
DartRoutingTableMaintenance::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  (reinterpret_cast<DartRoutingTableMaintenance*>(f))->table_maintenance();
  (reinterpret_cast<DartRoutingTableMaintenance*>(f))->set_lookup_timer();
}

void
DartRoutingTableMaintenance::set_lookup_timer()
{
  _lookup_timer.reschedule_after_msec( _update_interval );
}

/**
  * return the neighbour with the shortest ID
  * TODO: also considerer linkquality
  */

DHTnode*
DartRoutingTableMaintenance::getBestNeighbour()
{
  DHTnode *best = NULL;
  DHTnode *next = NULL;

  if ( _drt->_neighbours.size() == 0 ) return NULL;

//  best = _drt->_neighbours.get_dhtnode(0);
best = _drt->_neighbours[0]->neighbour;
  for ( int n = 1; n < _drt->_neighbours.size(); n++ ) {
    next = _drt->_neighbours[n]->neighbour;//.get_dhtnode(n);
    if ( next->_digest_length < best->_digest_length ) {
      //TODO: better metric (linkquality, age,.....
      best = next;
    }
  }

  return best;
}

void
DartRoutingTableMaintenance::table_maintenance()
{
  BRN_DEBUG("table maintenance");
  BRN_DEBUG("active:%s, validID:%s,neighbours:%d",String(_activestart).c_str(),String(_drt->_validID).c_str(),_drt->_neighbours.size());
  if ( ( _activestart ) && ( ! _drt->_validID ) && ( _drt->_neighbours.size() == 0 ) )  {
    //I'm active, have no valid ID and no neighbour to ask for an ID
    _drt->_me->set_nodeid(NULL, 0);             //I'm the first so set my ID
    _drt->_validID = true;
    BRN_DEBUG("Set my ID to 0");
    memcpy(_drt->_ident,_drt->_me->_ether_addr.data(),6);
    _drt->update_callback(DART_UPDATE_ID);      //update my own id

  } else if ( _drt->_neighbours.size() > 0 ) {  //I've neighbours, so check
    if ( ! _drt->_validID ) {                   //i've no ID, so ask best neighbour for ID(-Sharing)
      DHTnode *bestneighbour = getBestNeighbour();

      BRN_DEBUG("Ask for an ID cause I dont have one - Best Neighbour of %d : %s",_drt->_neighbours.size(),bestneighbour->_ether_addr.unparse().c_str());

      WritablePacket *p = DHTProtocolDart::new_nodeid_request_packet( _drt->_me, bestneighbour );
      output(0).push(p);
    } else {
      //TODO: check for pdate, Balancing,....
    }
  }
}

void
DartRoutingTableMaintenance::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    switch ( DHTProtocol::get_type(packet) ) {
      case DART_MINOR_REQUEST_ID:
        handle_request(packet);
        break;
      case DART_MINOR_ASSIGN_ID:
        handle_assign(packet);
        break;
      case DART_MINOR_REVOKE_ID:
      case DART_MINOR_UPDATE_ID:
      case DART_MINOR_RELEASE_ID:
      default:
        BRN_WARN("Unknown Operation in falcon-routing");
        packet->kill();
        break;
    }
  }
}

void
DartRoutingTableMaintenance::assign_id(DHTnode *newnode)
{
  DartFunctions::copy_id(newnode, _drt->_me);
  DartFunctions::append_id_bit(newnode, 1);
  DartFunctions::append_id_bit(_drt->_me, 0);
  BRN_DEBUG("\n <update time=\"%s\"></update>",Timestamp::now().unparse().c_str());
}

void
DartRoutingTableMaintenance::handle_request(Packet *packet)
{
  DHTnode src;

  uint8_t status;

  BRN_DEBUG("Got ID request");

  DHTProtocolDart::get_info(packet, &src, &status);

  if ( ! DartFunctions::has_max_id_length(_drt->_me) ) {
    assign_id(&src);
    WritablePacket *rep  = DHTProtocolDart::new_nodeid_assign_packet( _drt->_me, &src, packet,_drt->_ident); //reply, so change src and dst

    src._neighbor = true;         //Request only comes from neighbouring nodes //TODO: check

    _drt->update_node(&src);      //update the new node (add if not exists)

  //BRN_DEBUG("MAIN: Routingtable:\n%s",_drt->routing_info().c_str());

    output(0).push(rep);

      /* i change my own id. also a new/updated node is there. Inform evry element (storage,routing,...)*/
    _drt->update_callback(DART_UPDATE_ID);    //TODO: move this and the change of the own ID to routingtable

  } else {
    BRN_WARN("ID-space is full. No id left");
  }
}

void
DartRoutingTableMaintenance::handle_assign(Packet *packet)
{
  DHTnode src;
  DHTnode dst;

  uint8_t status;
  EtherAddress ident = EtherAddress();
  BRN_DEBUG("GOT ID REPLY");

  DHTProtocolDart::get_info(packet, &src, &dst, &status,&ident);
  memcpy(_drt->_ident,ident.data(),6);
  DartFunctions::copy_id(_drt->_me, &dst);
  _drt->_validID = true;
BRN_DEBUG("My new ID: %s ",DartFunctions::print_id(_drt->_me->_md5_digest,_drt->_me->_digest_length).c_str());
  _drt->update_node(&src);                //update src, which has a new id
  _drt->update_callback(DART_UPDATE_ID);  //Test: update my own address

  packet->kill();

}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

enum {
  H_ACTIVESTART
};

static int
write_param(const String &in_s, Element *e, void *thunk, ErrorHandler *errh)
{
  DartRoutingTableMaintenance *drt = reinterpret_cast<DartRoutingTableMaintenance*>(e);
  String s = cp_uncomment(in_s);

  switch ((uintptr_t) thunk)
  {
    case H_ACTIVESTART :
    {
      bool activestart;
      if (!cp_bool(s, &activestart))
        return errh->error("activestart parameter must be a bool value (false/true)");
      drt->_activestart = activestart;
      break;
    }
  }

  return 0;
}

static String
read_debug_param(Element *e, void *)
{
  DartRoutingTableMaintenance *drtm = reinterpret_cast<DartRoutingTableMaintenance*>(e);
  return String(drtm->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  DartRoutingTableMaintenance *drtm = reinterpret_cast<DartRoutingTableMaintenance*>(e);
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug))
    return errh->error("debug parameter must be an integer value between 0 and 4");
  drtm->_debug = debug;
  return 0;
}


void DartRoutingTableMaintenance::add_handlers()
{
  add_write_handler("activestart", write_param , (void *)H_ACTIVESTART);
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DartRoutingTableMaintenance)
