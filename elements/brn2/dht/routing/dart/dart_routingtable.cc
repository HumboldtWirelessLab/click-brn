#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/brn_md5.hh"
#include "dart_functions.hh"
#include "dart_routingtable.hh"

CLICK_DECLS

DartRoutingTable::DartRoutingTable():
  _validID(false)
{
}

DartRoutingTable::~DartRoutingTable()
{
}

int
DartRoutingTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  EtherAddress _my_ether_addr;

  if (cp_va_kparse(conf, this, errh,
    "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_my_ether_addr,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  _me = new DHTnode(_my_ether_addr,NULL,0); //NULL and 0 set the nodeid to zero
  _me->_status = STATUS_OK;
  _me->_neighbor = true;
  _allnodes.add_dhtnode(_me);

  return 0;
}

int
DartRoutingTable::initialize(ErrorHandler *)
{
  return 0;
}

/****************************************************************************************
********************* N O D E T A B L E O P E R A T I O N *******************************
****************************************************************************************/
int
DartRoutingTable::add_node(DHTnode *node)
{
  DHTnode *n;

  n = _allnodes.get_dhtnode(node);

  if ( n == NULL ) {
    n = node->clone();
    _allnodes.add_dhtnode(n);
    if ( n->_neighbor ) _neighbours.add_dhtnode(n);
  } else {
    DartFunctions::copy_id(n,node);
    //TODO: update rest of node
  }

  return 0;
}

int
DartRoutingTable::add_node(DHTnode *node, bool is_neighbour)
{
  node->_neighbor = is_neighbour;
  return add_node(node);
}

int
DartRoutingTable::add_neighbour(DHTnode *node)
{
  return add_node(node, true);
}

int
DartRoutingTable::add_nodes(DHTnodelist *nodes)
{
  DHTnode *n;

  for ( int i = 0; i < nodes->size(); i++ ) {
    n = nodes->get_dhtnode(i);
    add_node(n);
  }

  return 0;
}

DHTnode *
DartRoutingTable::get_node(EtherAddress *ea)
{
  return _allnodes.get_dhtnode(ea);
}

DHTnode *
DartRoutingTable::get_neighbour(EtherAddress *ea)
{
  return _neighbours.get_dhtnode(ea);
}

int
DartRoutingTable::update_node(DHTnode *node)
{
  return add_node(node);
}


/*************************************************************************************************/
/******************************** C A L L B A C K ************************************************/
/*************************************************************************************************/
int
DartRoutingTable::add_update_callback(void (*info_func)(void*,int), void *info_obj)
{
  _callbacklist.push_back(new CallbackFunction(info_func, info_obj));
  return 0;
}

void
DartRoutingTable::update_callback(int status)
{
  CallbackFunction *cbf;
  for ( int i = 0; i < _callbacklist.size(); i++ ) {
    cbf = _callbacklist[i];

    (*cbf->_info_func)(cbf->_info_obj, status);
  }
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String 
DartRoutingTable::routing_info(void)
{
  StringAccum sa;

  sa << "Routing Info ( Node: " << _me->_ether_addr.unparse() << " )\n";
  sa << "ID: " << DartFunctions::print_id(_me) << "   Valid ID: " << _validID << "\n";
  sa << "Neighbours (" << _neighbours.size() << "):\n";
  for ( int n = 0; n < _neighbours.size(); n++ )
    sa << " " << n << ": " << _neighbours.get_dhtnode(n)->_ether_addr.unparse() << " ID: " << DartFunctions::print_id(_neighbours.get_dhtnode(n)) << "\n";

  sa << "All Nodes (" << _allnodes.size() << "):\n";
  for ( int n = 0; n < _allnodes.size(); n++ )
    sa << " " << n << ": " << _allnodes.get_dhtnode(n)->_ether_addr.unparse() << " ID: " << DartFunctions::print_id(_allnodes.get_dhtnode(n)) << "\n";

  return sa.take_string();
}

enum {
  H_ROUTING_INFO
};

static String
read_param(Element *e, void *thunk)
{
  DartRoutingTable *drt = (DartRoutingTable *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( drt->routing_info( ) );
    default: return String();
  }
}

void DartRoutingTable::add_handlers()
{
  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DartRoutingTable)
