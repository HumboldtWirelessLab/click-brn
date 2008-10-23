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

void *DHTRoutingOmni::cast(const char *name)
{
  if (strcmp(name, "DHTRoutingOmni") == 0)
    return (DHTRoutingOmni *) this;
  else if (strcmp(name, "DHTRouting") == 0)
         return (DHTRouting *) this;
       else
         return NULL;
}

int DHTRoutingOmni::configure(Vector<String> &conf, ErrorHandler *errh)
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

int DHTRoutingOmni::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( _update_interval );

  return 0;
}

void DHTRoutingOmni::set_lookup_timer()
{
  _lookup_timer.schedule_after_msec( _update_interval );
}

void DHTRoutingOmni::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((DHTRoutingOmni*)f)->nodeDetection();
  ((DHTRoutingOmni*)f)->set_lookup_timer();
}

void DHTRoutingOmni::push( int port, Packet *packet )
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
      case HELLO_REQUEST:
              {
                handle_hello_request(packet);
                break;
              }
      default: click_chatter("Not implemented jet");
    }
  }

  packet->kill();

}

void DHTRoutingOmni::handle_hello_request(Packet *p)
{
  click_ether *ether_header = (click_ether*)p->ether_header();
  click_chatter("Got Hello Request from %s to %s. me is %s",EtherAddress(ether_header->ether_shost).unparse().c_str(), EtherAddress(ether_header->ether_dhost).unparse().c_str(),_me->_ether_addr.unparse().c_str());
  if ( is_me(ether_header->ether_dhost) ) click_chatter("is for me");
  p->kill();
}

void DHTRoutingOmni::nodeDetection()
{
  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist
  WritablePacket *p,*big_p;
  DHTnode *node;
  struct click_brn brn_header;
  click_ether *ether_header;

  if ( _linkstat == NULL ) return;

  _linkstat->get_neighbors(&neighbors);
  //click_chatter("Have %d neigh",neighbors.size());

  //Check for new neighbors
  for( int i = 0; i < neighbors.size(); i++ ) {
    node = _dhtnodes.get_dhtnode(&(neighbors[i]));
    if ( node == NULL ) {
      node = new DHTnode(neighbors[i]);
      node->_status = STATUS_UNKNOWN;
      _dhtnodes.add_dhtnode(node);

      p = DHTProtocolOmni::new_hello_request_packet(&(_me->_ether_addr));
      big_p = p->push(sizeof(struct click_brn) + sizeof(click_ether));

      if ( big_p == NULL ) {
        click_chatter("Push failed. No memory left ??");
      }
      else
      {
        ether_header = (click_ether *)p->data();
        memcpy( ether_header->ether_dhost,&(neighbors[i]),6);
        memcpy( ether_header->ether_shost,_me->_ether_addr.data(),6);
        ether_header->ether_type = htons(ETHERTYPE_BRN);
        p->set_ether_header(ether_header);

        brn_header.dst_port = 10;
        brn_header.src_port = 10;
        brn_header.body_length = p->length();
        brn_header.ttl = 100;
        brn_header.tos = 0;

        memcpy((void*)&(p->data()[14]),(void*)&brn_header, sizeof(brn_header));

        output(0).push(p);
      }
    }
  }

  //check for missing Neighbors
}

void DHTRoutingOmni::add_handlers()
{
}

#include <click/vector.cc>

template class Vector<DHTRoutingOmni::BufferedPacket>;

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolOmni)
EXPORT_ELEMENT(DHTRoutingOmni)
