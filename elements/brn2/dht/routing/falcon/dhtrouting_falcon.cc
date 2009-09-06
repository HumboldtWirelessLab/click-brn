#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "dhtrouting_falcon.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "elements/brn2/dht/standard/md5.h"
#include "dhtprotocol_falcon.hh"

CLICK_DECLS

DHTRoutingFalcon::DHTRoutingFalcon():
  _linkstat(NULL)
{
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
      "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_me,
      "LINKSTAT", cpkP+cpkM, cpElement, &_linkstat,
      cpEnd) < 0)
    return -1;

  return 0;
}

int DHTRoutingFalcon::initialize(ErrorHandler *)
{
  return 0;
}

void DHTRoutingFalcon::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) output(0).push(packet);
}

DHTnode *
DHTRoutingFalcon::get_responsibly_node(md5_byte_t *key)
{
//  click_chatter("Falcon gives node");
  return NULL;
}

void
DHTRoutingFalcon::nodeDetection()
{
  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist
  WritablePacket *p,*big_p;

  if ( (_linkstat == NULL) && (_fingertable.size() == 0) ) {  //active braodcast probing if no linkstat

    return;
  }

  _linkstat->get_neighbors(&neighbors);

  //Check for new neighbors
  for( int i = 0; i < neighbors.size(); i++ ) {
    click_chatter("New neighbors");

    p = DHTProtocolFalcon::new_route_request_packet(_me);
    big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &(neighbors[i]), BRN_PORT_DHTROUTING);

    if ( big_p == NULL ) click_chatter("Error in DHT");
    else output(0).push(big_p);
  }
}

String 
DHTRoutingFalcon::routing_info(void)
{
  StringAccum sa;
  DHTnode *node;
  uint32_t numberOfNodes = 0;
  char digest[16*2 + 1];

  numberOfNodes = _fingertable.size();
  if ( successor != NULL ) numberOfNodes++;
  if ( predecessor != NULL ) numberOfNodes++;

  MD5::printDigest(_me->_md5_digest, digest);
  sa << "Routing Info ( Node: " << _me->_ether_addr.unparse() << "\t" << digest << " )\n";
  sa << "DHT-Nodes (" << (int)numberOfNodes  << ") :\n";

  if ( successor != NULL ) {
    MD5::printDigest(successor->_md5_digest, digest);
    sa << "Successor: " << successor->_ether_addr.unparse() << "\t" << digest << "\n";
  }

  if ( predecessor != NULL ) {
    MD5::printDigest(predecessor->_md5_digest, digest);
    sa << "Predecessor: " << predecessor->_ether_addr.unparse() << "\t" << digest << "\n";
  }

  for( int i = 0; i < _fingertable.size(); i++ )
  {
    node = _fingertable.get_dhtnode(i);

    sa << node->_ether_addr.unparse();
    MD5::printDigest(node->_md5_digest, digest);

    sa << "\t" << digest;
    if ( node->_neighbor )
      sa << "\ttrue";
    else
      sa << "\tfalse";

    sa << "\t" << (int)node->_status;
    sa << "\t" << node->_age;
    sa << "\t" << node->_last_ping;

    sa << "\n";
  }

  return sa.take_string();
}


enum {
  H_ROUTING_INFO
};

static String
read_param(Element *e, void *thunk)
{
  DHTRoutingFalcon *dht_falcon = (DHTRoutingFalcon *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_falcon->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingFalcon::add_handlers()
{
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTRoutingFalcon)
