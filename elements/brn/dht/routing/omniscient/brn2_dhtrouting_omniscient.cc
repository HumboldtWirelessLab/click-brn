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
#include "brn2_dhtrouting_omniscient.hh"

#include "elements/brn/dht/protocol/dhtprotocol.hh"
#include "dhtprotocol_omniscient.hh"

#include "elements/brn/routing/nblist.hh"
#include "elements/brn/routing/linkstat/brnlinkstat.hh"

CLICK_DECLS

DHTRoutingOmni::DHTRoutingOmni():
  _lookup_timer(static_lookup_timer_hook,this)
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

  return 0;
}

int
DHTRoutingOmni::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( _update_interval );

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
DHTRoutingOmni::push( int port, Packet *packet )
{
  click_chatter("One Packet");
  if ( port == 0 )
  {
    click_chatter("Unicast");
    if ( DHTProtocol::get_routing(packet) != ROUTING_OMNI )
    {
       click_chatter("Not OMNI");
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

  click_chatter("Got Hello from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

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

  click_chatter("Got Hello Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
                  EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);

  if ( is_me(ether_header->ether_dhost) )
  {
    node = _dhtnodes.get_dhtnode(0);
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
  WritablePacket *p,*big_p;
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;
  DHTnode *node,*srcnode;
  DHTnodelist tmp_list;

  click_chatter("Got Hello Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
                EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);
  srcnode = dhtlist.get_dhtnode(0);  //TODO: replace by srcnode from header
  dhtlist.clear();

  for ( int i = 0; i < _dhtnodes.size(); i++ )
  {
    node = _dhtnodes.get_dhtnode(i);
    if ( node->_status == STATUS_OK )
    {
      tmp_list.add_dhtnode(node);

      if ( tmp_list.size() == 100 )  //TODO: which max length
      {
        p = DHTProtocolOmni::new_route_reply_packet(&(_me->_ether_addr), &tmp_list);
        big_p = DHTProtocolOmni::push_brn_ether_header(p, &(_me->_ether_addr), &(srcnode->_ether_addr));
        output(0).push(big_p);

        tmp_list.clear();
      }
    }
  }

  if ( tmp_list.size() > 0 )        //send the rest
  {
    p = DHTProtocolOmni::new_route_reply_packet(&(_me->_ether_addr), &tmp_list);
    big_p = DHTProtocolOmni::push_brn_ether_header(p, &(_me->_ether_addr), &(srcnode->_ether_addr));
    output(0).push(big_p);

    tmp_list.clear();
  }

}

void
DHTRoutingOmni::handle_routetable_reply(Packet *p_in)
{
  click_ether *ether_header = (click_ether*)p_in->ether_header();
  DHTnodelist dhtlist;
  int count_nodes;

  click_chatter("Got Hello Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(),
                EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());

  count_nodes = DHTProtocolOmni::get_dhtnodes(p_in, &dhtlist);
  update_nodes(&dhtlist);
  dhtlist.clear();
}

void
DHTRoutingOmni::send_routetable_update(EtherAddress *dst)
{
  WritablePacket *p,*big_p;
  DHTnode *node;
  DHTnodelist tmp_list;

  for ( int i = 0; i < _dhtnodes.size(); i++ )
  {
    node = _dhtnodes.get_dhtnode(i);
    if ( node->_status == STATUS_OK )
    {
      tmp_list.add_dhtnode(node);

      if ( tmp_list.size() == 100 )  //TODO: which max length
      {
        p = DHTProtocolOmni::new_route_reply_packet(&(_me->_ether_addr), &tmp_list);
        big_p = DHTProtocolOmni::push_brn_ether_header(p, &(_me->_ether_addr), dst);
        output(0).push(big_p);

        tmp_list.clear();
      }
    }
  }

  if ( tmp_list.size() > 0 )        //send the rest
  {
    p = DHTProtocolOmni::new_route_reply_packet(&(_me->_ether_addr), &tmp_list);
    big_p = DHTProtocolOmni::push_brn_ether_header(p, &(_me->_ether_addr), dst);
    output(0).push(big_p);

    tmp_list.clear();
  }

}

void
DHTRoutingOmni::update_nodes(DHTnodelist *dhtlist)
{
  DHTnode *node, *new_node;
  int count_newnodes = 0;

  for ( int i = 0; i < dhtlist->size(); i++)
  {
    new_node = dhtlist->get_dhtnode(i);
    node = _dhtnodes.get_dhtnode(new_node);
    if ( node == NULL )
    {
      node = new DHTnode(new_node->_ether_addr);
      _dhtnodes.add_dhtnode(node);
      count_newnodes++;
    }

    node->_status = STATUS_OK;
  }

  if ( count_newnodes > 0 )
  {

  }
}

void
DHTRoutingOmni::nodeDetection()
{
  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist
  WritablePacket *p,*big_p;
  DHTnode *node;

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
    if ( node->_neighbor )
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
}

void DHTRoutingOmni::add_handlers()
{
}

#include <click/vector.cc>

template class Vector<DHTRoutingOmni::BufferedPacket>;

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolOmni)
EXPORT_ELEMENT(DHTRoutingOmni)
