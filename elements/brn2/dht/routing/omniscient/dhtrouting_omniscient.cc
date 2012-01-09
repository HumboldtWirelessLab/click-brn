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

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/standard/packetsendbuffer.hh"
#include "elements/brn2/standard/brn_md5.hh"

#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#include "dhtrouting_omniscient.hh"
#include "dhtprotocol_omniscient.hh"

CLICK_DECLS

DHTRoutingOmni::DHTRoutingOmni():
  _ping_timer(static_ping_timer_hook,this),
  _lookup_timer(static_lookup_timer_hook,this),
  _packet_buffer_timer(static_packet_buffer_timer_hook,this)
{
  DHTRouting::init();
}

DHTRoutingOmni::~DHTRoutingOmni()
{
}

void *
DHTRoutingOmni::cast(const char *name)
{
  if (strcmp(name, "DHTRoutingOmni") == 0)
    return (DHTRoutingOmni *) this;
  else if (strcmp(name, "DHTRouting") == 0)
         return (DHTRouting *) this;
       else
         return NULL;
}

int
DHTRoutingOmni::configure(Vector<String> &conf, ErrorHandler *errh)
{
  EtherAddress _my_ether_addr;
  _linkstat = NULL;                          //no linkstat
  _update_interval = DHT_OMNI_DEFAULT_UPDATE_INTERVAL;                   //update interval -> 1 sec
  _start_delay = DHT_OMNI_DEFAULT_START_DELAY;

  if (cp_va_kparse(conf, this, errh,
    "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_my_ether_addr,
    "LINKSTAT", cpkP, cpElement, &_linkstat,
    "UPDATEINT", cpkP, cpInteger, &_update_interval,
    "STARTDELAY", cpkP, cpInteger, &_start_delay,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  if (!_linkstat || !_linkstat->cast("BRN2LinkStat"))
  {
    _linkstat = NULL;
    BRN_WARN("No Linkstat");
  }

  _me = new DHTnode(_my_ether_addr);
  _me->_status = STATUS_OK;
  _me->_neighbor = true;
  _dhtnodes.add_dhtnode(_me);

  return 0;
}

/**
 * Using this handler, we can use the linkprobes to distributed information
 * src is the Etheraddress of the source of the LP if lp is received (see direction)
 */

static int
tx_handler(void *element, const EtherAddress */*src*/, char */*buffer*/, int /*size*/)
{
  DHTRoutingOmni *dhtro = (DHTRoutingOmni*)element;
  if ( dhtro == NULL ) return 0;
//  return lph->lpSendHandler(buffer, size);
  return 0;
}

static int
rx_handler(void *element, EtherAddress */*src*/, char */*buffer*/, int /*size*/, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  DHTRoutingOmni *dhtro = (DHTRoutingOmni*)element;
  if ( dhtro == NULL ) return 0;
//  return lph->lpReceiveHandler(buffer, size);*/

  return 0;
}
int
DHTRoutingOmni::initialize(ErrorHandler *)
{
  click_srandom(_me->_ether_addr.hashcode());

  if ( _linkstat )
    _linkstat->registerHandler(this,0,&tx_handler,&rx_handler);

  _ping_timer.initialize(this);
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( _start_delay + ( click_random() % _update_interval ) );
  _packet_buffer_timer.initialize(this);

  return 0;
}

void
DHTRoutingOmni::set_lookup_timer()
{
  _lookup_timer.schedule_after_msec( _update_interval );
}

void
DHTRoutingOmni::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Timer is NULL");
  else {
    ((DHTRoutingOmni*)f)->nodeDetection();
    ((DHTRoutingOmni*)f)->set_lookup_timer();
  }
}

void
DHTRoutingOmni::static_packet_buffer_timer_hook(Timer *t, void *f)
{
  DHTRoutingOmni *dht;
  PacketSendBuffer::BufferedPacket *bpacket;
  int next_p;

  dht = (DHTRoutingOmni*)f;

  if ( t == NULL ) {
    click_chatter("Timer is NULL");
    return;
  }
  bpacket = dht->packetBuffer.getNextBufferedPacket();

  if ( bpacket != NULL )
  {
    dht->output(bpacket->_port).push(bpacket->_p);
    next_p = dht->packetBuffer.getTimeToNext();
    if ( next_p >= 0 )
      dht->_packet_buffer_timer.schedule_after_msec( next_p );

    delete bpacket;
  }
}

void
DHTRoutingOmni::static_ping_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Timer is NULL");
  else ((DHTRoutingOmni*)f)->ping_timer();
}

void
DHTRoutingOmni::push( int port, Packet *packet )
{

  if ( port == 0 )
  {
    if ( DHTProtocol::get_routing(packet) != ROUTING_OMNI )
    {
       packet->kill();
       return;
    }

    switch (DHTProtocol::get_type(packet))
    {
      case OMNI_HELLO:
              {
                handle_hello(packet);
                break;
              }
      case OMNI_HELLO_REQUEST:
              {
                handle_hello_request(packet);
                break;
              }
      case OMNI_ROUTETABLE_REQUEST:
              {
                handle_routetable_request(packet);
                break;
              }
      case OMNI_ROUTETABLE_REPLY:
              {
                handle_routetable_reply(packet);
                break;
              }
      default: BRN_ERROR("Not implemented jet");
    }
  }

  packet->kill();

}

void
DHTRoutingOmni::handle_hello(Packet *p_in)
{
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;

  EtherAddress ea;
  DHTnode *node;

  BRN_DEBUG("Got Hello from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);

  dhtlist.del();

  ea = EtherAddress(ether_header->ether_shost);
  node = _dhtnodes.get_dhtnode(&ea);
  node->set_age_now();

}

void
DHTRoutingOmni::handle_hello_request(Packet *p_in)
{
  WritablePacket *p,*big_p; 
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;

  EtherAddress ea;
  DHTnode *node;

  BRN_DEBUG("Got Hello Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);

  if ( is_me(ether_header->ether_dhost) )
  {
    node = dhtlist.get_dhtnode(0);
    p = DHTProtocolOmni::new_hello_packet(&(_me->_ether_addr));
    big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &(node->_ether_addr), BRN_PORT_DHTROUTING);

    if ( big_p == NULL ) {
      BRN_WARN("Push failed. No memory left ??");
      p->kill();
    } else
      output(0).push(big_p);
  }

  dhtlist.del();

  /** TODO: move up to send new information */
  ea = EtherAddress(ether_header->ether_shost);
  node = _dhtnodes.get_dhtnode(&ea);
  node->set_age_now();

}

void
DHTRoutingOmni::handle_routetable_request(Packet *p_in)
{
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;
  EtherAddress ea;

  BRN_DEBUG("Got Route Table Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
                EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  ea = EtherAddress(ether_header->ether_shost);
  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);
  dhtlist.clear();

  send_routetable_update(&ea, STATUS_UNKNOWN );

}

void
DHTRoutingOmni::handle_routetable_reply(Packet *p_in)
{
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;

  BRN_DEBUG("Got Route Table reply from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
             EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);

  dhtlist.del();
}

void
DHTRoutingOmni::send_routetable_update(EtherAddress *dst, int status)
{
  WritablePacket *p,*big_p;
  DHTnode *node;
  DHTnodelist tmp_list;
  int jitter, next_p;

  for ( int i = 0; i < _dhtnodes.size(); i++ )
  {
    node = _dhtnodes.get_dhtnode(i);
    if ( ( node->_status == status ) || ( status == STATUS_UNKNOWN ) )
    {
      if ( node->_status == STATUS_NEW ) node->_status = STATUS_OK;

      tmp_list.add_dhtnode(node);

      if ( ((tmp_list.size() + 1) * sizeof(struct dht_omni_node_entry)) > DHT_OMNI_MAX_PACKETSIZE_ROUTETABLE )//TODO: which max length
      {
        p = DHTProtocolOmni::new_route_reply_packet(&(_me->_ether_addr), &tmp_list);
        big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), dst, BRN_PORT_DHTROUTING);

        jitter = (unsigned int ) ( click_random() % 50 );
        packetBuffer.addPacket_ms(big_p, jitter, 0);

        tmp_list.clear();
      }
    }
  }

  if ( tmp_list.size() > 0 )        //send the rest
  {
    p = DHTProtocolOmni::new_route_reply_packet(&(_me->_ether_addr), &tmp_list);
    big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), dst, BRN_PORT_DHTROUTING);

    jitter = (unsigned int ) ( click_random() % 50 );
    packetBuffer.addPacket_ms(big_p, jitter, 0);

    tmp_list.clear();
  }

  next_p = packetBuffer.getTimeToNext();
  _packet_buffer_timer.schedule_after_msec( next_p );

}

void
DHTRoutingOmni::send_routetable_request(EtherAddress *dst)
{
  WritablePacket *p,*big_p;
  DHTnode *node;
  DHTnodelist tmp_list;

  BRN_DEBUG("%s: Send request to %s",_me->_ether_addr.unparse().c_str(), dst->unparse().c_str());

  for ( int i = 0; i < _dhtnodes.size(); i++ )
  {
    node = _dhtnodes.get_dhtnode(i);
    if ( node->_status == STATUS_OK )
    {

      tmp_list.add_dhtnode(node);

      if ( ((tmp_list.size() + 1) * sizeof(struct dht_omni_node_entry)) > DHT_OMNI_MAX_PACKETSIZE_ROUTETABLE )//TODO: which max length
      {
        p = DHTProtocolOmni::new_route_request_packet(&(_me->_ether_addr), &tmp_list);
        big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), dst, BRN_PORT_DHTROUTING);

        BRN_DEBUG("Out");
        output(0).push(big_p);

        tmp_list.clear();
      }
    }
  }

  if ( tmp_list.size() > 0 )
  {
    p = DHTProtocolOmni::new_route_request_packet(&(_me->_ether_addr), &tmp_list);
    big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), dst, BRN_PORT_DHTROUTING);

    BRN_DEBUG("Out");
    output(0).push(big_p);

    tmp_list.clear();
  }
}

/****************************************************************************************
****************************** N O D E   F O R   K E Y *+********************************
****************************************************************************************/
DHTnode *
DHTRoutingOmni::get_responsibly_node_for_key(md5_byte_t *key)
{
  DHTnode *node;

  if ( key == NULL ) return NULL;

  for ( int i = 0; i < _dhtnodes.size(); i++ )
  {
    node = _dhtnodes.get_dhtnode(i);
    if ( ( MD5::hexcompare( node->_md5_digest, key ) >= 0 ) && ( node->_status == STATUS_OK) )
      return node;
  }

  for ( int i = 0; i < _dhtnodes.size(); i++ ) {
    node = _dhtnodes.get_dhtnode(i);
    if ( node->_status == STATUS_OK )
      return node;
  }

  return NULL;

}

DHTnode *
DHTRoutingOmni::get_responsibly_node(md5_byte_t *key, int replica_number)
{
  if ( replica_number == 0 ) get_responsibly_node_for_key(key);

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
void
DHTRoutingOmni::update_nodes(DHTnodelist *dhtlist)
{
  DHTnode *node, *new_node;
  int count_newnodes = 0;
  int add_nodes = 0;
  Timestamp now,n_age;

  now = Timestamp::now();

  for ( int i = 0; i < dhtlist->size(); i++)
  {
    new_node = dhtlist->get_dhtnode(i);
    node = _dhtnodes.get_dhtnode(new_node);

    if ( node == NULL )
    {
      if ( new_node->_status == STATUS_OK )
      {
        node = new DHTnode(new_node->_ether_addr);                //TODO: use the nodes from list directly (save new-operation, but than check handle_routetable_reply
        _dhtnodes.add_dhtnode(node);
        count_newnodes++;
        add_nodes++;
        node->_status = STATUS_NEW;
        node->_neighbor = false;                                  //TODO: take these info from node direct
        node->set_age(&(new_node->_age));
        node->set_last_ping(&(new_node->_last_ping));
      }
    }
    else
    {
      if ( new_node->_status == STATUS_AWAY )
      {
        if ( node->get_age_s() >= new_node->get_age_s() )    //his info is newer
        {
          node->_status = STATUS_AWAY;                      //mark node as away and set ping timestamp
        }
      }
      else
      {
        if ( node->_status == STATUS_UNKNOWN )
        {
          count_newnodes++;
          node->_status = STATUS_NEW;
          node->set_age(&(new_node->_age));
          node->set_last_ping(&(new_node->_last_ping));
        }
        else
        {
          if ( node->get_age_s() >= new_node->get_age_s() )    //his info is newer
          {
            if ( new_node->get_age_s() <= node->get_last_ping_s() )
              if ( node->_status == STATUS_MISSED || node->_status == STATUS_AWAY )
              {
                node->_status = STATUS_OK;                    //he says node is ok
                node->set_age(&(new_node->_age));
                node->set_last_ping(&(new_node->_last_ping));
                //remove now unneeded ping and so
              }
          }
        }
      }
    }
  }

  if ( count_newnodes > 0 )
  {
    if ( add_nodes > 0 ) {
      _dhtnodes.sort();
      _ping_timer.schedule_after_msec(_update_interval);
    }

    EtherAddress broadcast = EtherAddress::make_broadcast();
    send_routetable_update(&broadcast, STATUS_NEW);

    notify_callback(ROUTING_STATUS_NEW_NODE);
  }
}

void
DHTRoutingOmni::nodeDetection()
{
  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist
  WritablePacket *p,*big_p;
  DHTnode *node;
  int add_nodes = 0;

  if ( _linkstat == NULL ) return;

  _linkstat->get_neighbors(&neighbors);

  //Check for new neighbors
  for( int i = 0; i < neighbors.size(); i++ ) {
    node = _dhtnodes.get_dhtnode(&(neighbors[i]));
    if ( node == NULL ) {
      BRN_DEBUG("New neighbors");
      node = new DHTnode(neighbors[i]);
      node->_status = STATUS_UNKNOWN;
      node->_neighbor = true;
      _dhtnodes.add_dhtnode(node);
      add_nodes++;

      p = DHTProtocolOmni::new_hello_request_packet(&(_me->_ether_addr));
      big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &(neighbors[i]), BRN_PORT_DHTROUTING);

      if ( big_p == NULL ) {
        BRN_WARN("Error in DHT");
        p->kill();
      } else
        output(0).push(big_p);
    }
    else
    {
      node->_status = STATUS_OK;
      node->_neighbor = true;
    }
  }

  //check for missing Neighbors
  for( int i = 0; i < _dhtnodes.size(); i++ )
  {
    node = _dhtnodes.get_dhtnode(i);
    if ( node->_neighbor && node != _me)
    {
      int j;

      for ( j = 0; j < neighbors.size(); j++ )
        if ( node->_ether_addr == neighbors[j] ) break;

      if ( j == neighbors.size() )  //dhtneighbor is not my neighbor now
      {
        node->_status = STATUS_MISSED;
        node->_neighbor = false;

        p = DHTProtocolOmni::new_hello_request_packet(&(_me->_ether_addr));
        big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &(node->_ether_addr), BRN_PORT_DHTROUTING);

        if ( big_p == NULL ) {
          BRN_WARN("Error in DHT");
        } else output(0).push(big_p);
      }
    }
  }

  if ( add_nodes > 0 ) _dhtnodes.sort();

}

void
DHTRoutingOmni::ping_timer()
{
  DHTnode *node = NULL;

  _me->set_last_ping_now();                             //set my own pingtime to so i don't choose myself

  node = _dhtnodes.get_dhtnode_oldest_ping();

  if ( node != NULL ) {
    DHTRoutingOmni::send_routetable_request(&(node->_ether_addr));
    node->set_last_ping_now();
  }

  _ping_timer.schedule_after_msec( _update_interval );

}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String 
DHTRoutingOmni::routing_info(void)
{
  StringAccum sa;
  DHTnode *node;
  char digest[16*2 + 1];

  sa << "Routing Info ( Node: " << _me->_ether_addr.unparse() << " )\n";
  sa << "DHT-Nodes (" << (int)_dhtnodes.size() << ") :\n";

  for( int i = 0; i < _dhtnodes.size(); i++ )
  {
    node = _dhtnodes.get_dhtnode(i);

    sa << node->_ether_addr.unparse();
    MD5::printDigest(node->_md5_digest, digest);

    sa << "\t" << digest;
    if ( node->_neighbor )
      sa << "\ttrue";
    else
      sa << "\tfalse";

    sa << "\t" << node->get_status_string();
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
  DHTRoutingOmni *dht_omni = (DHTRoutingOmni *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_omni->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingOmni::add_handlers()
{
  DHTRouting::add_handlers();

  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolOmni)
EXPORT_ELEMENT(DHTRoutingOmni)
