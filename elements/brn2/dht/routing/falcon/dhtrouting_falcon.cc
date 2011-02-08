#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "falcon_functions.hh"
#include "falcon_routingtable.hh"

#include "dhtrouting_falcon.hh"

CLICK_DECLS

DHTRoutingFalcon::DHTRoutingFalcon():
  _frt(NULL),
  _leave_organizer(NULL),
  _responsible(FALCON_RESPONSIBLE_FORWARD)
{
  DHTRouting::init();
}

DHTRoutingFalcon::~DHTRoutingFalcon()
{
}

void *DHTRoutingFalcon::cast(const char *name)
{
  if (strcmp(name, "DHTRoutingFalcon") == 0)
    return (DHTRoutingFalcon *) this;
  else if (strcmp(name, "DHTRouting") == 0)
         return (DHTRouting *) this;
       else
         return NULL;
}

int DHTRoutingFalcon::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "LEAVEORGANIZER", cpkP, cpElement, &_leave_organizer,
      "RESPONSIBLE", cpkP, cpInteger, &_responsible,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  _me = _frt->_me;

  return 0;
}

static void notify_callback_func(void *e, int status)
{
  DHTRoutingFalcon *f = (DHTRoutingFalcon *)e;
  f->handle_routing_update_callback(status);
}

int DHTRoutingFalcon::initialize(ErrorHandler *)
{
  _frt->add_update_callback(notify_callback_func,(void*)this);
  return 0;
}

/**
 * This version is Chord-like: a node is responsible for a key if the key is placed between the node and its predecessor
 */

DHTnode *
DHTRoutingFalcon::get_responsibly_node_backward(md5_byte_t *key)
{
  DHTnode *best;

  if ( ( _frt->successor == NULL ) || ( _frt->_fingertable.size() == 0 ) ) return _frt->_me;

  if ( FalconFunctions::is_in_between( _frt->predecessor, _frt->_me, key) ||
       FalconFunctions::is_equals(_frt->_me, key) ) return _frt->_me;
  if ( FalconFunctions::is_in_between( _frt->_me, _frt->successor, key) ||
       FalconFunctions::is_equals(_frt->successor, key) ) return _frt->successor; //TODO: this should be handle
                                                                                  //by checking the FT

  best = _frt->successor;      //default

  for ( int i = ( _frt->_fingertable.size() - 1 ); i >= 0; i-- ) {
    if ( ! FalconFunctions::is_in_between( _frt->_me, _frt->_fingertable.get_dhtnode(i), key) ) {
      best = _frt->_fingertable.get_dhtnode(i);
      break;
    }
  }

  //check this first and not the fingertable, since all nodes includes also the FT-node
  for ( int i = ( _frt->_allnodes.size() - 1 ); i >= 0; i-- ) { 
    if ( FalconFunctions::is_in_between( best, key, _frt->_allnodes.get_dhtnode(i) ) ||
         FalconFunctions::is_equals( _frt->_allnodes.get_dhtnode(i), key ) ) {
      best = _frt->_allnodes.get_dhtnode(i);
    }
  }

  return best;
}

/**
 * a node is responsible for a key if the key is placed between the node and its successor
 */

DHTnode *
DHTRoutingFalcon::get_responsibly_node_forward(md5_byte_t *key)
{
  DHTnode *best;

  if ( ( _frt->successor == NULL ) || ( _frt->_fingertable.size() == 0 ) ) return _frt->_me;

  if ( FalconFunctions::is_in_between( _frt->_me, _frt->successor, key ) ||
       FalconFunctions::is_equals( _frt->_me, key ) ) return _frt->_me;  //TODO: this should be handle by checking the FT

  if ( FalconFunctions::is_in_between( _frt->predecessor, _frt->_me, key) ||
       FalconFunctions::is_equals( _frt->predecessor, key ) ) return _frt->predecessor;

  best = _frt->successor;      //default

  for ( int i = ( _frt->_fingertable.size() - 1 ); i >= 0; i-- ) {
    if ( ! FalconFunctions::is_in_between( _frt->_me, _frt->_fingertable.get_dhtnode(i), key) )  {
      best = _frt->_fingertable.get_dhtnode(i);
      break;
    }
  }

  for ( int i = ( _frt->_allnodes.size() - 1 ); i >= 0; i-- ) {  //check this first and not the fingertable, since all nodes includes also the FT-node
    if ( FalconFunctions::is_in_between( best, key, _frt->_allnodes.get_dhtnode(i) ) ||
         FalconFunctions::is_equals( _frt->_allnodes.get_dhtnode(i), key ) ) {
      best = _frt->_allnodes.get_dhtnode(i);
    }
  }

  return best;
}

DHTnode *
DHTRoutingFalcon::get_responsibly_node(md5_byte_t *key, int replica_number)
{
  if ( replica_number == 0 ) return get_responsibly_node_for_key(key);

  uint8_t r,r_swap;
  md5_byte_t replica_key[MAX_NODEID_LENTGH];

  memcpy(replica_key, key, MAX_NODEID_LENTGH);
  r = replica_number;
  r_swap = 0;

  for( int i = 0; i <= 7; i++ ) r_swap |= ((r >> i) & 1) << (7 - i);
  replica_key[0] ^= r_swap;

  return get_responsibly_node_for_key(replica_key);
}

DHTnode *
DHTRoutingFalcon::get_responsibly_node_for_key(md5_byte_t *key)
{
  if ( _responsible == FALCON_RESPONSIBLE_CHORD ) return get_responsibly_node_backward(key);
  return get_responsibly_node_forward(key);
}

void
DHTRoutingFalcon::range_query_min_max_id(uint8_t *min, uint8_t *max)
{
  if ( _responsible == FALCON_RESPONSIBLE_CHORD ) {
    memcpy( min, _frt->predecessor, 16 );
    memcpy( max, _frt->_me, 16 );
  } else {
    memcpy( min, _frt->_me, 16 );
    memcpy( max, _frt->successor, 16 );
  }
}

int
DHTRoutingFalcon::change_node_id(md5_byte_t *id, int id_len)
{
  if ( _frt->successor == NULL ) {
    BRN_WARN("Change nodeid without send any information. BE careful.");
    _frt->_me->set_nodeid(id);
    _frt->_me->_status = STATUS_OK;

    return CHANGE_NODE_ID_STATUS_OK;
  }

  if ( ! _leave_organizer ) return DHTRouting::change_node_id(id, id_len);

  if ( _leave_organizer->start_leave(id, id_len) ) return CHANGE_NODE_ID_STATUS_OK;

  return CHANGE_NODE_ID_STATUS_ONGOING_CHANGE;
}

/**
 * This Function can be called by upper layers (e.g. Storage) to update the node (age)
 */
int
DHTRoutingFalcon::update_node(EtherAddress */*ea*/, md5_byte_t */*id*/, int /*id_len*/)
{
  return 0;
}


/*************************************************************************************************/
/******************************** C A L L B A C K ************************************************/
/*************************************************************************************************/

void
DHTRoutingFalcon::handle_routing_update_callback(int status)
{
  if ( ( ( _responsible == FALCON_RESPONSIBLE_CHORD ) && ( status == RT_UPDATE_PREDECESSOR ) ) ||
       ( ( _responsible == FALCON_RESPONSIBLE_FORWARD ) && ( status == RT_UPDATE_SUCCESSOR ) ) )
    notify_callback(ROUTING_STATUS_NEW_NODE | ROUTING_STATUS_NEW_CLOSE_NODE);
}

/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

enum {
  H_ROUTING_INFO
};

static String
read_param(Element *e, void *thunk)
{
  DHTRoutingFalcon *dhtf = (DHTRoutingFalcon *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dhtf->_frt->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingFalcon::add_handlers()
{
  DHTRouting::add_handlers();

  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTRoutingFalcon)
