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

#include "elements/brn2/brnconf.h"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/standard/packetsendbuffer.hh"
#include "elements/brn2/standard/brn_md5.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "dhtrouting_klibs.hh"
#include "dhtprotocol_klibs.hh"


CLICK_DECLS

DHTRoutingKlibs::DHTRoutingKlibs():
  _lookup_timer(static_lookup_timer_hook,this),
  _packet_buffer_timer(static_packet_buffer_timer_hook,this)
{
  DHTRouting::init();
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
  _max_age = 10;                             //don't use nodes which are older than 10 sec
  _start_time = 10000;
  _max_own_nodes_per_packet = 30;
  _max_foreign_nodes_per_packet = 3;
  _max_foreign_nodes = 3;

  if (cp_va_kparse(conf, this, errh,
    "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_my_ether_addr,
    "LINKSTAT", cpkP, cpElement, &_linkstat,
    "STARTTIME", cpkP, cpInteger, &_start_time,
    "UPDATEINT", cpkP, cpInteger, &_update_interval,
    "MAXAGE", cpkP, cpInteger, &_max_age,
    "MAXPINGTIME", cpkP, cpInteger, &_max_ping_time,
    "MAXOWNNODESPERPACKET", cpkP, cpInteger, &_max_own_nodes_per_packet,
    "MAXFOREIGNNODESPERPACKET", cpkP, cpInteger, &_max_foreign_nodes_per_packet,
    "MAXFOREIGNNODESTORE", cpkP, cpInteger, &_max_foreign_nodes,
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

/**
 * src is the Etheraddress of the source of the LP if lp is received (see direction)
 */

static int
tx_handler(void *element, const EtherAddress */*src*/, char */*buffer*/, int /*size*/)
{
  DHTRoutingKlibs *dhtrk = (DHTRoutingKlibs*)element;
  if ( dhtrk == NULL ) return 0;

  //return lph->lpSendHandler(buffer, size);

  return 0;
}

static int
rx_handler(void *element, EtherAddress */*src*/, char */*buffer*/, int /*size*/, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  DHTRoutingKlibs *dhtrk = (DHTRoutingKlibs*)element;
  if ( dhtrk == NULL ) return 0;

  //return lph->lpReceiveHandler(buffer, size);*/

  return 0;
}

int
DHTRoutingKlibs::initialize(ErrorHandler *)
{
  click_srandom(_me->_ether_addr.hashcode());

  if ( _linkstat )
    _linkstat->registerHandler(this,0,&tx_handler,&rx_handler);

  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( click_random() % _start_time );
  _packet_buffer_timer.initialize(this);
  _packet_buffer_timer.schedule_after_msec(DEFAULT_SENDPUFFER_TIMEOUT);

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
  bpacket = dht->packetBuffer.getNextBufferedPacket();

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
//  click_ether *ether_header = (click_ether*)p_in->ether_header();
//  click_chatter("Got Hello from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
//                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  DHTnodelist dhtlist;
//  int count_nodes;
  DHTnode *me_from_list;
  uint8_t ptype;
  /*count_nodes =*/ DHTProtocolKlibs::get_dhtnodes(p_in, &ptype, &dhtlist);
  EtherAddress ea = EtherAddress(DHTProtocol::get_src_data(p_in));
  DHTnode *node;
  bool notify_storage = false;

  node = _foreign_dhtnodes.get_dhtnode(&ea);
  if ( node == NULL ) {
    node = _own_dhtnodes.get_dhtnode(&ea);
  }
  if ( node != NULL ) {
    node->set_age_now();
    node->_status = STATUS_OK;
  } else {
    node = new DHTnode(ea);
    node->_status = STATUS_OK;
    node->_neighbor = false;
    node->set_age_now();

    if ( is_own(node) ) {
      _own_dhtnodes.add_dhtnode(node);
      _own_dhtnodes.sort();
      notify_storage = true;
    } else {
      if ( _foreign_dhtnodes.size() == 0 ) notify_storage = true; //notify storage only if we have the first foreign node
      _foreign_dhtnodes.add_dhtnode(node);
      _foreign_dhtnodes.sort();
      if ( _foreign_dhtnodes.size() > _max_foreign_nodes ) {
        DHTnode *oldest = _foreign_dhtnodes.get_dhtnode_oldest_age();
        _foreign_dhtnodes.erase_dhtnode(&(oldest->_ether_addr));
      }
    }
  }

  if ( ( me_from_list = dhtlist.get_dhtnode(_me) ) != NULL ) {
    node->set_last_ping_s(me_from_list->get_age_s());
  }

//  click_chatter("Nodes: %d",count_nodes);
  notify_storage |= update_nodes(&dhtlist);

  dhtlist.del();

  if ( notify_storage ) notify_callback(ROUTING_STATUS_NEW_NODE);
}

void
DHTRoutingKlibs::sendHello()
{
  WritablePacket *p,*big_p;
  DHTnodelist dhtlist;
  DHTnode *node = NULL;

  unsigned int group = click_random() % 10;

  _me->set_last_ping_now();                             //set my own pingtime to so i don't choose myself

  if ( ( group < 3 ) && ( _foreign_dhtnodes.size() != 0 )) {
    node = _foreign_dhtnodes.get_dhtnode_oldest_ping();
  } else {
    if ( _own_dhtnodes.size() > 1 )                     //Only if I'm not the only one
      node = _own_dhtnodes.get_dhtnode_oldest_ping();
  }

  if ( node == NULL ) return;   //no node to ping

  get_nodelist(&dhtlist, node, ALL_NODES);
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
//  int count_nodes;
  DHTnode *node = NULL;
  bool notify_storage = false;

//  click_chatter("Got Hello Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
//                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());
  uint8_t ptype;
  /*count_nodes =*/ DHTProtocolKlibs::get_dhtnodes(p_in, &ptype, &dhtlist);
  notify_storage = update_nodes(&dhtlist);

  if ( is_me(ether_header->ether_dhost) )
  {
    EtherAddress ea = EtherAddress(ether_header->ether_shost);

    node = _own_dhtnodes.get_dhtnode(&ea);       //TODO: better check whether node is foreign or own
    if ( node == NULL )
      node = _foreign_dhtnodes.get_dhtnode(&ea);

    if ( node != NULL ) {
      node = new DHTnode(ea);
      node->_status = STATUS_OK;
      node->_neighbor = false;
      node->set_age_now();

      if ( is_own(node) ) {
        _own_dhtnodes.add_dhtnode(node);
        _own_dhtnodes.sort();
        notify_storage = true;
      } else {
        if ( _foreign_dhtnodes.size() == 0 ) notify_storage = true; //notify storage only if we have the first foreign node
        _foreign_dhtnodes.add_dhtnode(node);
        _foreign_dhtnodes.sort();
        if ( _foreign_dhtnodes.size() > _max_foreign_nodes ) {
          DHTnode *oldest = _foreign_dhtnodes.get_dhtnode_oldest_age();
          _foreign_dhtnodes.erase_dhtnode(&(oldest->_ether_addr));
        }
      }
    }

    get_nodelist(&send_dhtlist, node, node_group);
    p = DHTProtocolKlibs::new_packet(&(_me->_ether_addr),&ea, KLIBS_HELLO, &send_dhtlist);
    big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &ea, BRN_PORT_DHTROUTING);
    send_dhtlist.clear();

    if ( big_p == NULL ) click_chatter("Push failed. No memory left ??");
    else output(0).push(big_p);
  }

  dhtlist.del();

  if ( notify_storage ) notify_callback(ROUTING_STATUS_NEW_NODE);
}

/****************************************************************************************
****************************** N O D E   F O R   K E Y *+********************************
****************************************************************************************/
DHTnode *
DHTRoutingKlibs::get_responsibly_node_for_key(md5_byte_t *key)
{
  DHTnode *node;

  if ( key == NULL ) return NULL;

  if ( is_own(key) ) {

    for ( int i = 0; i < _own_dhtnodes.size(); i++ )
    {
      node = _own_dhtnodes.get_dhtnode(i);
      if ( ( MD5::hexcompare( node->_md5_digest, key ) >= 0 ) && ( node->_status == STATUS_OK) && ( node->get_age_s() <= _max_age ) )
        return node;
    }

    for ( int i = 0; i < _own_dhtnodes.size(); i++ )
    {
      node = _own_dhtnodes.get_dhtnode(i);
      if ( ( node->_status == STATUS_OK ) && ( node->get_age_s() <= _max_age ) )
        return node;
    }

    click_chatter("No responsible node, but i should be in the list");

  } else {
    if ( _foreign_dhtnodes.size() != 0 ) {  //first  try the foreign

      for ( int i = 0; i < _foreign_dhtnodes.size(); i++ )
      {
        node = _foreign_dhtnodes.get_dhtnode(i);
        if ( ( MD5::hexcompare( node->_md5_digest, key ) >= 0 ) && ( node->_status == STATUS_OK ) && ( node->get_age_s() <= _max_age ) )
          return node;
      }

      for ( int i = 0; i < _foreign_dhtnodes.size(); i++ )
      {
        node = _foreign_dhtnodes.get_dhtnode(i);
        if ( ( node->_status == STATUS_OK ) && ( node->get_age_s() <= _max_age ) )
          return node;
      }
    }

    md5_byte_t new_key[16];
    memcpy(new_key,key,16);
    new_key[0] ^= 128;

    return get_responsibly_node_for_key(new_key);
  }

  click_chatter("no node");
  return NULL;
}

DHTnode *
DHTRoutingKlibs::get_responsibly_node(md5_byte_t *key, int replica_number)
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
bool
DHTRoutingKlibs::update_nodes(DHTnodelist *dhtlist)
{
  DHTnode *node, *new_node;
  int add_own_nodes = 0;
  int add_for_nodes = 0;
  bool own_group;
  bool notify_storage = false;

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
        notify_storage = true;
      } else {
        if ( _foreign_dhtnodes.size() == 0 ) notify_storage = true; //notify storage only if we have the first foreign node
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
//    click_chatter("i know the nodes");
      if ( node->get_age_s() >= new_node->get_age_s() )    //his info is newer
        node->set_age(&(new_node->_age));
    }
  }

  if ( add_own_nodes > 0 ) _own_dhtnodes.sort();
  if ( add_for_nodes > 0 ) {
    while ( _foreign_dhtnodes.size() > _max_foreign_nodes )
    {
      DHTnode *oldest = _foreign_dhtnodes.get_dhtnode_oldest_age();
      _foreign_dhtnodes.erase_dhtnode(&(oldest->_ether_addr));
    }

    _foreign_dhtnodes.sort();
  }

  return notify_storage;
}

void
DHTRoutingKlibs::nodeDetection()
{
  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist
  WritablePacket *p,*big_p;
  DHTnode *node;
  DHTnodelist _list;
  bool _list_filled = false;

//  bool is_own;


  if ( _linkstat == NULL ) return;

  _linkstat->get_neighbors(&neighbors);

  //Check for new neighbors
  for( int i = 0; i < neighbors.size(); i++ ) {
//    click_chatter("New neighbors");

    node = _own_dhtnodes.get_dhtnode(&(neighbors[i]));       //TODO: better check whether node is foreign or own

    if ( node == NULL ) {
//      is_own = false;
      node = _foreign_dhtnodes.get_dhtnode(&(neighbors[i]));
    } /*else {
      is_own = true;
    }*/

    if ( node == NULL ) {
      //Don't add the node. Just send him a packet with routing information. if it is a dht-node
      //it will send a packet anyway TODO: think about this, you also can use broadcast if one or more neighbors are new
      if ( ! _list_filled ) {
        get_nodelist(&_list, NULL, ALL_NODES);
        _list_filled = true;
      }

      p = DHTProtocolKlibs::new_packet(&(_me->_ether_addr),&(neighbors[i]), KLIBS_HELLO, &_list);
      big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &(neighbors[i]), BRN_PORT_DHTROUTING);

      if ( big_p == NULL ) click_chatter("Error in DHT");
      else output(0).push(big_p);
    }
    else
    {
      node->_status = STATUS_OK;
      node->_neighbor = true;
    }
  }

  if ( _list_filled ) _list.clear();

}

/*******************************************************************************************/
/******** C R E A T E   N O D E L I S T   F O R   O T H E R ********************************/
/*******************************************************************************************/
//TODO: send only the newest one
//TODO: performanceimproment by adding me to list. is it neassesarry ???
/**
 * _dst is the dst of the dht list. if this is null it is a broadcast-paket, then ignore it
 * if not not null, insert information about the node.
 * Performanceimprovment (doing that):
 * if a neighbour of mine got a hello from the _dst, he update the age
 * if my neighbour now send me this information, i now the actual age of _dst.
 * if i now send _dst this information, which i got inderectly, he can update the _last_ping for me
 * it like multicast_hello ( send it to one, and this one will distribute the info and so ping several nodes)
 *
 * Part of this function is implemted in handleHello
*/
int
DHTRoutingKlibs::get_nodelist(DHTnodelist *list, DHTnode *_dst, uint32_t group)
{
  int max_own_node;
  int max_for_node;
#ifdef BRNDEBUG_DHT_ROUTING_KLIBS
  int added_nodes = 0;
#endif

  if ( ( group == ALL_NODES ) || ( group = OWN_NODES ) )
  {
    if ( _own_dhtnodes.size() > _max_own_nodes_per_packet ) max_own_node = _max_own_nodes_per_packet;
    else max_own_node = _own_dhtnodes.size();

    for ( int i = 0; i < max_own_node; i++, _p_own_dhtnodes = ( _p_own_dhtnodes + 1 ) % _own_dhtnodes.size() )
    {
      list->add_dhtnode(_own_dhtnodes.get_dhtnode(_p_own_dhtnodes));
#ifdef BRNDEBUG_DHT_ROUTING_KLIBS
      added_nodes++;
#endif
    }
  }

  if ( ( group == ALL_NODES ) || ( group = FOREIGN_NODES ) )
  {
    if ( _foreign_dhtnodes.size() > _max_foreign_nodes_per_packet ) max_for_node = _max_foreign_nodes_per_packet;
    else max_for_node = _foreign_dhtnodes.size();

    for ( int i = 0; i < max_for_node; i++, _p_foreign_dhtnodes = ( _p_foreign_dhtnodes + 1 ) % _foreign_dhtnodes.size() )
    {
      list->add_dhtnode(_foreign_dhtnodes.get_dhtnode(_p_foreign_dhtnodes));
#ifdef BRNDEBUG_DHT_ROUTING_KLIBS
      added_nodes++;
#endif
    }
  }

  if ( _dst != NULL ) {                   //the dest of the packet should ( if it is not NULL) should be include in the packte
    if ( ! list->contains(_dst) ) {       //so the desz knowns what i know about him
      list->add_dhtnode(_dst);
    }
  }

#ifdef BRNDEBUG_DHT_ROUTING_KLIBS
  click_chatter("Add %d nodes to packet",added_nodes);
#endif

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
  DHTRoutingKlibs *dht_klibs = (DHTRoutingKlibs *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_klibs->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingKlibs::add_handlers()
{
  DHTRouting::add_handlers();

  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolKlibs)
EXPORT_ELEMENT(DHTRoutingKlibs)
