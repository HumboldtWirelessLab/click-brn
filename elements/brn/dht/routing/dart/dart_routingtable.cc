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

DHTnode*
DartRoutingTable::add_neighbours_neighbour(DHTnode* neighbour,DHTnodelist* neighbours){

DHTnode* node;
DHTnode* new_node;

DHTnodelist* neighbourlist;
for(int i= 0;i<_neighbours.size();i++){
  if (memcmp(_neighbours[i]->neighbour->_ether_addr.data(),neighbour->_ether_addr.data(),6) == 0)
   {
     BRN_DEBUG("found neihgbour: %s, add his neighbours",_neighbours[i]->neighbour->_ether_addr.unparse().c_str());
     neighbourlist = _neighbours[i]->neighbours_neighbour;
     for(int n = 0;n<neighbours->size();n++){
	 node = neighbours->get_dhtnode(n);
	 if (neighbourlist->get_dhtnode(&(node->_ether_addr)) == NULL){
          //get the reference from _allnodes 
          new_node = _allnodes.get_dhtnode(&(node->_ether_addr));
	  neighbourlist->add_dhtnode(new_node);
     }
    }

  return _neighbours[i]->neighbour;
  }  
 
}

return NULL;
}
DartRoutingTable::DRTneighbour*
DartRoutingTable::add_neighbour_entry(DHTnode* new_neighbour){
 
  DartRoutingTable::DRTneighbour *n = new DRTneighbour(new_neighbour);
  _neighbours.push_back(n);
  return n;
}

int
DartRoutingTable::add_node(DHTnode *node)
{
  DHTnode *n;

  n = _allnodes.get_dhtnode(node);

  if ( n == NULL ) {
    n = node->clone();
    _allnodes.add_dhtnode(n);
 if ( n->_neighbor )// _neighbours.add_dhtnode(n);
 add_neighbour_entry(n);
  } else {
    	if(n->_age <= node->_age){
     DartFunctions::copy_id(n,node);
     n->_age = node->_age;
    }

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
 for(int i= 0;i<_neighbours.size();i++){
  if (memcmp(_neighbours[i]->neighbour->_ether_addr.data(),ea->data(),6) == 0)
    return _neighbours[i]->neighbour;
}
return NULL;
 
//return _neighbours.get_dhtnode(ea);
}

int
DartRoutingTable::update_node(DHTnode *node)
{
  node->_age = Timestamp::now();
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

  sa << "<dartroutinginfo addr=\"" << _me->_ether_addr.unparse() << "\" id=\"" << DartFunctions::print_raw_id(_me);
  sa << "\" id_len=\"" << _me->_digest_length << "\" valid_id=\"" << String(_validID) << "\" >\n\t<neighbours count=\"";
  sa << _neighbours.size() << "\" >\n";
  for ( int n = 0; n < _neighbours.size(); n++ ) {
    DHTnode *node = _neighbours[n]->neighbour;
    sa << "\t\t<neighbour addr=\"" << node->_ether_addr.unparse() << "\" id=\"";
    sa << DartFunctions::print_raw_id(node) << "\" id_len=\"" << node->_digest_length << "\" />\n";
 sa << "\t\t\t<neighbours_neighbour count=\"";
  sa << _neighbours[n]->neighbours_neighbour->size() << "\" >\n";
    for (int i = 0 ; i <_neighbours[n]->neighbours_neighbour->size();i++){
	DHTnode *neighb = _neighbours[n]->neighbours_neighbour->get_dhtnode(i);
	sa << "\t\t\t\t<neighbour addr=\"" << neighb->_ether_addr.unparse() << "\" id=\"";
        sa << DartFunctions::print_raw_id(neighb) << "\" id_len=\"" << neighb->_digest_length << "\" />\n";	
     }
   sa << "\t\t\t</neighbours_neighbour>\n";
  }

sa << "\t</neighbours>\n\t<allnodes count=\"" << _allnodes.size() << "\" >\n";
  for ( int n = 0; n < _allnodes.size(); n++ ) {
    DHTnode *node = _allnodes.get_dhtnode(n);
    sa << "\t\t<node addr=\"" << node->_ether_addr.unparse() << "\" id=\"";
    sa << DartFunctions::print_raw_id(node) << "\" id_len=\"" << node->_digest_length << "\" />\n";
  }

  sa << "\t</allnodes>\n</dartroutinginfo>\n";

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
