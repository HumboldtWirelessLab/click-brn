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
#include "dhtrouting_omniscient.hh"

#include "elements/brn/standard/packetsendbuffer.hh"

#include "elements/brn/dht/protocol/dhtprotocol.hh"
#include "dhtprotocol_omniscient.hh"

#include "elements/brn/routing/nblist.hh"
#include "elements/brn/routing/linkstat/brnlinkstat.hh"

CLICK_DECLS

DHTRoutingOmni::DHTRoutingOmni():
  _lookup_timer(static_lookup_timer_hook,this),
  _packet_buffer_timer(static_packet_buffer_timer_hook,this)
{
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
  _update_interval = 1000;                   //update interval -> 1 sec

  if (cp_va_kparse(conf, this, errh,
    "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_my_ether_addr,
    "LINKSTAT", cpkP+cpkM, cpElement, &_linkstat,
    "UPDATEINT", cpkP+cpkM, cpInteger, &_update_interval,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  if (!_linkstat || !_linkstat->cast("BRNLinkStat"))
  {
    _linkstat = NULL;
    click_chatter("kein Linkstat");
  }

  _me = new DHTnode(_my_ether_addr);
  _me->_status = STATUS_OK;
  _me->_neighbor = true;
  _dhtnodes.add_dhtnode(_me);

  return 0;
}

int
DHTRoutingOmni::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( 5000 + _update_interval );
  _packet_buffer_timer.initialize(this);
  _packet_buffer_timer.schedule_after_msec( 10000 );

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
  if ( t == NULL ) click_chatter("Time is NULL");
  ((DHTRoutingOmni*)f)->nodeDetection();
  ((DHTRoutingOmni*)f)->set_lookup_timer();
}

void
DHTRoutingOmni::static_packet_buffer_timer_hook(Timer *t, void *f)
{
  DHTRoutingOmni *dht;
  PacketSendBuffer::BufferedPacket *bpacket;
  int next_p;

  dht = (DHTRoutingOmni*)f;

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
  }
  else
  {
    dht->_packet_buffer_timer.schedule_after_msec( 10000 );
  }
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
      case HELLO:
              {
                handle_hello(packet);
                break;
              }
      case HELLO_REQUEST:
              {
                handle_hello_request(packet);
                break;
              }
      case ROUTETABLE_REQUEST:
              {
                handle_routetable_request(packet);
                break;
              }
      case ROUTETABLE_REPLY:
              {
                handle_routetable_reply(packet);
                break;
              }
      default: click_chatter("Not implemented jet");
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

//  click_chatter("Got Hello from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
//                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);

  dhtlist.clear();
}

void
DHTRoutingOmni::handle_hello_request(Packet *p_in)
{
  WritablePacket *p,*big_p; 
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;
  DHTnode *node;

//  click_chatter("Got Hello Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
//                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);

  if ( is_me(ether_header->ether_dhost) )
  {
    node = dhtlist.get_dhtnode(0);
    p = DHTProtocolOmni::new_hello_packet(&(_me->_ether_addr));
    big_p = DHTProtocolOmni::push_brn_ether_header(p, &(_me->_ether_addr), &(node->_ether_addr));

    if ( big_p == NULL ) click_chatter("Push failed. No memory left ??");
    else output(0).push(big_p);
  }

  dhtlist.clear();
}

void
DHTRoutingOmni::handle_routetable_request(Packet *p_in)
{
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;
  DHTnode *srcnode;
  DHTnodelist tmp_list;

//  click_chatter("Got Route Table Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
//                EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);
  srcnode = dhtlist.get_dhtnode(0);  //TODO: replace by srcnode from header
  dhtlist.clear();

  send_routetable_update(&(srcnode->_ether_addr), STATUS_ALL );

}

void
DHTRoutingOmni::handle_routetable_reply(Packet *p_in)
{
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;

//  click_chatter("GotRoute reply from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
//       EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);
  dhtlist.clear();
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
    if ( ( node->_status == status ) || ( status == STATUS_ALL ) )
    {
      if ( node->_status == STATUS_NEW ) node->_status = STATUS_OK;

      tmp_list.add_dhtnode(node);

      if ( tmp_list.size() == 100 )  //TODO: which max length
      {
        p = DHTProtocolOmni::new_route_reply_packet(&(_me->_ether_addr), &tmp_list);
        big_p = DHTProtocolOmni::push_brn_ether_header(p, &(_me->_ether_addr), dst);

        jitter = (unsigned int ) ( click_random() % 500 );
        packetBuffer.addPacket_ms(big_p, jitter, 0);
        next_p = packetBuffer.getTimeToNext();
        _packet_buffer_timer.schedule_after_msec( next_p );
        //output(0).push(big_p);

        tmp_list.clear();
      }
    }
  }

  if ( tmp_list.size() > 0 )        //send the rest
  {
    p = DHTProtocolOmni::new_route_reply_packet(&(_me->_ether_addr), &tmp_list);
    big_p = DHTProtocolOmni::push_brn_ether_header(p, &(_me->_ether_addr), dst);
    jitter = (unsigned int ) ( click_random() % 500 );
    packetBuffer.addPacket_ms(big_p, jitter, 0);
    next_p = packetBuffer.getTimeToNext();
    _packet_buffer_timer.schedule_after_msec( next_p );
    //output(0).push(big_p);

    tmp_list.clear();
  }

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
        node = new DHTnode(new_node->_ether_addr);
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
    if ( add_nodes > 0 ) _dhtnodes.sort();

    EtherAddress broadcast = EtherAddress::make_broadcast();
    DHTRoutingOmni::send_routetable_update(&broadcast, STATUS_NEW);
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
      node = new DHTnode(neighbors[i]);
      node->_status = STATUS_UNKNOWN;
      node->_neighbor = true;
      _dhtnodes.add_dhtnode(node);
      add_nodes++;

      p = DHTProtocolOmni::new_hello_request_packet(&(_me->_ether_addr));
      big_p = DHTProtocolOmni::push_brn_ether_header(p, &(_me->_ether_addr), &(neighbors[i]));

      if ( big_p == NULL ) click_chatter("Error in DHT");
      else output(0).push(big_p);
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
        big_p = DHTProtocolOmni::push_brn_ether_header(p, &(_me->_ether_addr), &(node->_ether_addr));

        if ( big_p == NULL ) click_chatter("Error in DHT");
        else output(0).push(big_p);
      }
    }
  }

  if ( add_nodes > 0 ) _dhtnodes.sort();

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
  DHTRoutingOmni *dht_omni = (DHTRoutingOmni *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_omni->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingOmni::add_handlers()
{
  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolOmni)
EXPORT_ELEMENT(DHTRoutingOmni)
