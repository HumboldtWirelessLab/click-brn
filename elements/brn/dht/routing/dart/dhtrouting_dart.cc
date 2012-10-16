/* OMNISCIENT
  DHT knows all othe nodes in the network. For Discovery, flooding is used.
  Everytime the routingtable of a node changed, it floods all new information.
  Node-fault detection is done by neighboring nodes
*/
#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/packetsendbuffer.hh"
#include "elements/brn2/standard/brn_md5.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#include "dhtrouting_dart.hh"
#include "dhtprotocol_dart.hh"

#include "dart_routingtable.hh"

#include "dart_functions.hh"

CLICK_DECLS

DHTRoutingDart::DHTRoutingDart()
{
  DHTRouting::init();
}

DHTRoutingDart::~DHTRoutingDart()
{
}

void *
DHTRoutingDart::cast(const char *name)
{
  if (strcmp(name, "DHTRoutingDart") == 0)
    return (DHTRoutingDart *) this;
  else if (strcmp(name, "DHTRouting") == 0)
         return (DHTRouting *) this;
       else
         return NULL;
}

int
DHTRoutingDart::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_kparse(conf, this, errh,
    "DRT", cpkP+cpkM , cpElement, &_drt,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  _me = _drt->_me;

  return 0;
}

int
DHTRoutingDart::initialize(ErrorHandler *)
{
  _drt->add_update_callback(routingtable_callback_func, this);
  return 0;
}

void
DHTRoutingDart::routingtable_callback_func(void *e, int status)
{
  DHTRoutingDart *s = (DHTRoutingDart *)e;

  if ( status == DART_UPDATE_ID)
    s->notify_callback(ROUTING_STATUS_NEW_NODE);
}

/****************************************************************************************
****************************** N O D E   F O R   K E Y *+********************************
****************************************************************************************/
DHTnode *
DHTRoutingDart::get_responsibly_node_for_key(md5_byte_t *key)
{
//  int diffbit;
  DHTnode *best_node = NULL;
  int position_best_node;
  DHTnode *acnode;
  int position_ac_node;

  BRN_DEBUG("Search for ID: %s",DartFunctions::print_id(key, 128).c_str());

  if ( DartFunctions::equals(_drt->_me, key) ) {
    BRN_DEBUG("It's me");
    return _drt->_me;
  }

  /*diffbit =*/ DartFunctions::diff_bit(_drt->_me, key);

  for ( int n = 0; n < _drt->_neighbours.size(); n++ ) {
    acnode = _drt->_neighbours.get_dhtnode(n);
    if ( DartFunctions::equals(acnode, key) ) {
      BRN_DEBUG("have full node");
      return acnode;
    }

    position_ac_node = DartFunctions::position_last_1(acnode);
    if ( DartFunctions::equals(acnode, key, position_ac_node ) && ((best_node == NULL) || (position_best_node < position_ac_node) ) ) {
      position_best_node = position_ac_node,
      best_node = acnode;
    }
  }


  if ( best_node == NULL ) {
    //click_chatter("Search for shortest");
    for ( int n = 0; n < _drt->_neighbours.size(); n++ ) {
      acnode = _drt->_neighbours.get_dhtnode(n);
      position_ac_node = DartFunctions::position_last_1(acnode);
      if ( (best_node == NULL) || (position_best_node > position_ac_node) ) {
        position_best_node = position_ac_node,
        best_node = acnode;
      }
    }
  }

  //TODO: this should never happen so check it dispensable
  if ( best_node == NULL ) {
    BRN_WARN("No node for id found. So use default.");
    best_node = _drt->_me;
  }

  return best_node;
}

DHTnode *
DHTRoutingDart::get_responsibly_node(md5_byte_t *key, int replica_number)
{
  if ( replica_number == 0 ) return get_responsibly_node_for_key(key);

  uint8_t r,r_swap;
  md5_byte_t replica_key[MAX_NODEID_LENTGH];

  memcpy(replica_key, key, MAX_NODEID_LENTGH);
  r = replica_number;
  r_swap = 0;

  for( int i = 0; i < 8; i++ ) r_swap |= ((r >> i) & 1) << (7 - i);
  replica_key[0] ^= r_swap;

  return get_responsibly_node_for_key(replica_key);
}

/****************************************************************************************
********************* N O D E T A B L E O P E R A T I O N *******************************
****************************************************************************************/

int
DHTRoutingDart::update_node(EtherAddress *ea, md5_byte_t *key, int keylen)
{
  DHTnode node(*ea, key, keylen);
  _drt->add_node(&node);

  //TODO: call update in drt to inform other elements ???

  return 0;
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

enum {
  H_ROUTING_INFO
};

static String
read_param(Element *e, void *thunk)
{
  DHTRoutingDart *dht_dart = (DHTRoutingDart *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_dart->_drt->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingDart::add_handlers()
{
  DHTRouting::add_handlers();

  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolDart)
EXPORT_ELEMENT(DHTRoutingDart)
