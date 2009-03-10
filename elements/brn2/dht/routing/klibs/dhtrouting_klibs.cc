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

#include "dhtrouting_klibs.hh"
#include "elements/brn2/standard/packetsendbuffer.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "elements/brn/dht/md5.h"
#include "dhtprotocol_klibs.hh"

#include "elements/brn/routing/nblist.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

CLICK_DECLS

DHTRoutingKlibs::DHTRoutingKlibs():
  _lookup_timer(static_lookup_timer_hook,this),
  _packet_buffer_timer(static_packet_buffer_timer_hook,this)
{
}

DHTRoutingKlibs::~DHTRoutingKlibs()
{
}

void *
DHTRoutingKlibs::cast(const char *name)
{
  if (strcmp(name, "DHTRoutingKlibs") == 0)
    return (DHTRoutingKlibs *) this;
  else if (strcmp(name, "DHTRouting") == 0)
         return (DHTRouting *) this;
       else
         return NULL;
}

int
DHTRoutingKlibs::configure(Vector<String> &conf, ErrorHandler *errh)
{
  EtherAddress _my_ether_addr;
  _linkstat = NULL;                          //no linkstat
  _update_interval = 1000;                   //update interval -> 1 sec

  if (cp_va_kparse(conf, this, errh,
    "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_my_ether_addr,
    "LINKSTAT", cpkP, cpElement, &_linkstat,
    "UPDATEINT", cpkP, cpInteger, &_update_interval,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  if (!_linkstat || !_linkstat->cast("BRN2LinkStat"))
  {
    _linkstat = NULL;
    click_chatter("No Linkstat");
  }

  _me = new DHTnode(_my_ether_addr);
  _me->_status = STATUS_OK;
  _me->_neighbor = true;
  _own_dhtnodes.add_dhtnode(_me);

  return 0;
}

int
DHTRoutingKlibs::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( ( click_random() % 10000 ) + _update_interval );
  _packet_buffer_timer.initialize(this);
  _packet_buffer_timer.schedule_after_msec( 10000 );

  return 0;
}

void
DHTRoutingKlibs::set_lookup_timer()
{
  _lookup_timer.schedule_after_msec( _update_interval );
}

void
DHTRoutingKlibs::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((DHTRoutingKlibs*)f)->nodeDetection();
  ((DHTRoutingKlibs*)f)->sendHello();
  ((DHTRoutingKlibs*)f)->set_lookup_timer();
}

void
DHTRoutingKlibs::static_packet_buffer_timer_hook(Timer *t, void *f)
{
  DHTRoutingKlibs *dht;
  PacketSendBuffer::BufferedPacket *bpacket;
  int next_p;

  dht = (DHTRoutingKlibs*)f;

  if ( t == NULL ) click_chatter("Timer is NULL");
  bpacket = dht->packetBuffer.getNextPacket();

  if ( bpacket != NULL )
  {
    dht->output(bpacket->_port).push(bpacket->_p);
    next_p = dht->packetBuffer.getTimeToNext();
    if ( next_p >= 0 )
      dht->_packet_buffer_timer.schedule_after_msec( next_p );
    else
      dht->_packet_buffer_timer.schedule_after_msec( 10000 );

    delete bpacket;
  }
  else
  {
    dht->_packet_buffer_timer.schedule_after_msec( 10000 );
  }
}

void
DHTRoutingKlibs::push( int port, Packet *packet )
{

  if ( port == 0 )
  {
    if ( DHTProtocol::get_routing(packet) != ROUTING_KLIBS )
    {
       packet->kill();
       return;
    }

    switch (DHTProtocol::get_type(packet))
    {
      case KLIBS_HELLO:
              {
                handle_hello(packet);
                break;
              }
      case KLIBS_REQUEST:
              {
                handle_request(packet,ALL_NODES);
                break;
              }
      case KLIBS_REQUEST_OWN:
              {
                handle_request(packet,OWN_NODES);
                break;
              }
      case KLIBS_REQUEST_FOREIGN:
              {
                handle_request(packet,FOREIGN_NODES);
                break;
              }
      default: click_chatter("Not implemented jet");
    }
  }

  packet->kill();

}

void
DHTRoutingKlibs::handle_hello(Packet *p_in)
{
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;

//  click_chatter("Got Hello from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
//                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  uint8_t ptype;
  count_nodes = DHTProtocolKlibs::get_dhtnodes(p_in, &ptype, &dhtlist);
  EtherAddress *ea = DHTProtocol::get_src(p_in);
  DHTnode *node;
  if ( ea != NULL ) {
    node = _foreign_dhtnodes.get_dhtnode(ea);
    if ( node == NULL ) {
      node = _own_dhtnodes.get_dhtnode(ea);
    }
    if ( node != NULL )                             //TODO: add soure if not in list
      node->set_age_now();
    else {
      node = new DHTnode(*ea);
      if ( is_own(node) ) {
        _own_dhtnodes.add_dhtnode(node);
      } else {
        _foreign_dhtnodes.add_dhtnode(node);
      }

      node->_status = STATUS_OK;
      node->_neighbor = false;                                  //TODO: take these info from node direct
      node->set_age_now();
    }

    delete ea;
  }
//  click_chatter("Nodes: %d",count_nodes);
  update_nodes(&dhtlist);

  dhtlist.del();
}

void
DHTRoutingKlibs::sendHello()
{
  WritablePacket *p,*big_p;
  DHTnodelist dhtlist;
  DHTnode *node;

  unsigned int group = click_random() % 10;

  _me->set_last_ping_now();

  if ( ( group < 3 ) && ( _foreign_dhtnodes.size() != 0 )) {
    node = _foreign_dhtnodes.get_dhtnode_oldest_ping();;
  } else {
    node = _own_dhtnodes.get_dhtnode_oldest_ping();
 }

 if ( node->_ether_addr == _me->_ether_addr )
 {
 //  click_chatter("don't send to myself");
   return;
 }

  get_nodelist(&dhtlist, ALL_NODES);
  p = DHTProtocolKlibs::new_packet(&(_me->_ether_addr),&(node->_ether_addr), KLIBS_HELLO, &dhtlist);
  big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &(node->_ether_addr), BRN_PORT_DHTROUTING);
  dhtlist.clear();

  if ( big_p == NULL ) click_chatter("Push failed. No memory left ??");
  else {
    node->set_last_ping_now();
    output(0).push(big_p);
  }
}


void
DHTRoutingKlibs::handle_request(Packet *p_in, uint32_t node_group)
{
  WritablePacket *p,*big_p; 
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  DHTnodelist send_dhtlist;
  int count_nodes;
//  DHTnode *node;

//  click_chatter("Got Hello Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
//                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());
  uint8_t ptype;
  count_nodes = DHTProtocolKlibs::get_dhtnodes(p_in, &ptype, &dhtlist);
  update_nodes(&dhtlist);

  get_nodelist(&send_dhtlist, node_group);

  if ( is_me(ether_header->ether_dhost) )
  {
    EtherAddress ea = EtherAddress(ether_header->ether_shost);
    p = DHTProtocolKlibs::new_packet(&(_me->_ether_addr),&ea, KLIBS_HELLO, &send_dhtlist);
    big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &ea, BRN_PORT_DHTROUTING);
    send_dhtlist.clear();

    if ( big_p == NULL ) click_chatter("Push failed. No memory left ??");
    else output(0).push(big_p);
  }

  dhtlist.del();
}

/****************************************************************************************
****************************** N O D E   F O R   K E Y *+********************************
****************************************************************************************/
DHTnode *
DHTRoutingKlibs::get_responsibly_node(md5_byte_t *key)
{
  DHTnode *node;

  if ( key == NULL ) return NULL;

  if ( is_own(key) ) {

    for ( int i = 0; i < _own_dhtnodes.size(); i++ )
    {
      node = _own_dhtnodes.get_dhtnode(i);
      if ( ( MD5::hexcompare( node->_md5_digest, key ) >= 0 ) && ( node->_status == STATUS_OK) )
        return node;
    }

    for ( int i = 0; i < _own_dhtnodes.size(); i++ )
    {
      node = _own_dhtnodes.get_dhtnode(i);
      if ( node->_status == STATUS_OK)
        return node;
    }

    click_chatter("No responsible node, but i should be in the list");

  } else {
    if ( _foreign_dhtnodes.size() != 0 ) {  //first  try the foreign

      for ( int i = 0; i < _foreign_dhtnodes.size(); i++ )
      {
        node = _foreign_dhtnodes.get_dhtnode(i);
        if ( ( MD5::hexcompare( node->_md5_digest, key ) >= 0 ) && ( node->_status == STATUS_OK) )
          return node;
      }

      for ( int i = 0; i < _foreign_dhtnodes.size(); i++ )
      {
        node = _foreign_dhtnodes.get_dhtnode(i);
        if ( node->_status == STATUS_OK)
          return node;
      }
    }

    md5_byte_t new_key[16];
    memcpy(new_key,key,16);
    new_key[0] ^= 128;

    return get_responsibly_node(new_key);
  }

  click_chatter("no node");
  return NULL;
}

bool
DHTRoutingKlibs::is_foreign(md5_byte_t *key)
{
  return ( ( _me->_md5_digest[0] & 128 ) != ( key[0] & 128 ) );
}

bool
DHTRoutingKlibs::is_own(md5_byte_t *key)
{
  return ( ( _me->_md5_digest[0] & 128 ) == ( key[0] & 128 ) );
}

bool
DHTRoutingKlibs::is_foreign(DHTnode *node)
{
  return ( ( _me->_md5_digest[0] & 128 ) != ( node->_md5_digest[0] & 128 ) );
}

bool
DHTRoutingKlibs::is_own(DHTnode *node)
{
  return ( ( _me->_md5_digest[0] & 128 ) == ( node->_md5_digest[0] & 128 ) );
}


/****************************************************************************************
********************* N O D E T A B L E O P E R A T I O N *******************************
****************************************************************************************/
void
DHTRoutingKlibs::update_nodes(DHTnodelist *dhtlist)
{
  DHTnode *node, *new_node;
  int add_own_nodes = 0;
  int add_for_nodes = 0;
  bool own_group;

  Timestamp now,n_age;

  now = Timestamp::now();

  for ( int i = 0; i < dhtlist->size(); i++)
  {
    new_node = dhtlist->get_dhtnode(i);
    own_group = is_own(new_node);
    if ( own_group )
      node = _own_dhtnodes.get_dhtnode(new_node);
    else
      node = _foreign_dhtnodes.get_dhtnode(new_node);


    if ( node == NULL )
    {
//      click_chatter("Unknown Node");
      node = new DHTnode(new_node->_ether_addr); //TODO: use the nodes from list directly (save
                                                 // new-operation, but than check handle_routetable_reply
      if ( own_group ) {
        _own_dhtnodes.add_dhtnode(node);
        add_own_nodes++;
      } else {
        _foreign_dhtnodes.add_dhtnode(node);
        add_for_nodes++;
      }

      node->_status = STATUS_OK;
      node->_neighbor = false;                                  //TODO: take these info from node direct
      node->set_age(&(new_node->_age));
      node->set_last_ping(&(new_node->_last_ping));
    }
    else
    {
//      click_chatter("i know the nodes");
      if ( node->get_age_s() >= new_node->get_age_s() )    //his info is newer
        node->set_age(&(new_node->_age));
    }
  }

  if ( add_own_nodes > 0 ) _own_dhtnodes.sort();
  if ( add_for_nodes > 0 ) _foreign_dhtnodes.sort();
}

void
DHTRoutingKlibs::nodeDetection()
{
  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist
  WritablePacket *p,*big_p;
  DHTnode *node;
  DHTnodelist _list;

  bool is_own;

  if ( _linkstat == NULL ) return;

  _linkstat->get_neighbors(&neighbors);

  //Check for new neighbors
  for( int i = 0; i < neighbors.size(); i++ ) {
//    click_chatter("New neighbors");

    node = _own_dhtnodes.get_dhtnode(&(neighbors[i]));       //TODO: better check whether node is foreign or own

    if ( node == NULL ) {
      is_own = false;
      node = _foreign_dhtnodes.get_dhtnode(&(neighbors[i]));
    } else {
      is_own = true;
    }

    if ( node == NULL ) {
      //Don't add the node. Just send him a packet with routing information. if it is a dht-node
      //it will send a packet anyway
      get_nodelist(&_list, ALL_NODES);
      p = DHTProtocolKlibs::new_packet(&(_me->_ether_addr),&(neighbors[i]), KLIBS_HELLO, &_list);
      big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &(neighbors[i]), BRN_PORT_DHTROUTING);
      _list.clear();

      if ( big_p == NULL ) click_chatter("Error in DHT");
      else output(0).push(big_p);
    }
    else
    {
      node->_status = STATUS_OK;
      node->_neighbor = true;
    }
  }
}
/*******************************************************************************************/
/******** C R E A T E   N O D E L I S T   F O R   O T H E R ********************************/
/*******************************************************************************************/

int
DHTRoutingKlibs::get_nodelist(DHTnodelist *list, uint32_t group)
{
  int max_own_node;
  int max_for_node;
  int added_nodes = 0;

  if ( ( group == ALL_NODES ) || ( group = OWN_NODES ) )
  {
    if ( _own_dhtnodes.size() > 15 ) max_own_node = 15;
    else max_own_node = _own_dhtnodes.size();

    for ( int i = 0; i < max_own_node; i++, _p_own_dhtnodes = ( _p_own_dhtnodes + 1 ) % _own_dhtnodes.size() )
    {
      list->add_dhtnode(_own_dhtnodes.get_dhtnode(i));
      added_nodes++;
    }
  }

  if ( ( group == ALL_NODES ) || ( group = FOREIGN_NODES ) )
  {
    if ( _foreign_dhtnodes.size() > 5 ) max_for_node = 5;
    else max_for_node = _foreign_dhtnodes.size();

    for ( int i = 0; i < max_for_node; i++, _p_foreign_dhtnodes = ( _p_foreign_dhtnodes + 1 ) % _foreign_dhtnodes.size() )
    {
      list->add_dhtnode(_foreign_dhtnodes.get_dhtnode(i));
      added_nodes++;
    }
  }

//  click_chatter("Add %d nodes to packet",added_nodes);
  return 0;
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String 
DHTRoutingKlibs::routing_info(void)
{
  StringAccum sa;
  DHTnode *node;
  char digest[16*2 + 1];

  sa << "Routing Info ( Node: " << _me->_ether_addr.unparse() << " )\n";
  sa << "DHT-Nodes (" << (int)_own_dhtnodes.size() +(int)_foreign_dhtnodes.size()  << ") :\n";

  sa << "Own\n";
  for( int i = 0; i < _own_dhtnodes.size(); i++ )
  {
    node = _own_dhtnodes.get_dhtnode(i);

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

  sa << "Foreign\n";

  for( int i = 0; i < _foreign_dhtnodes.size(); i++ )
  {
    node = _foreign_dhtnodes.get_dhtnode(i);

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
  DHTRoutingKlibs *dht_omni = (DHTRoutingKlibs *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_omni->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingKlibs::add_handlers()
{
  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolKlibs)
EXPORT_ELEMENT(DHTRoutingKlibs)
