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

#include "elements/brn/routing/nblist.hh"
#include "elements/brn/routing/linkstat/brnlinkstat.hh"
#include "elements/brn/dht/dhtcommunication.hh"


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
  _linkstat = NULL;                          //no linkstat
  _update_interval = 1000;                   //update interval -> 1 sec

  if (cp_va_kparse(conf, this, errh,
    "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_me,
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
    if ( port == 0 )
      packet->kill();
}


void DHTRoutingOmni::nodeDetection()
{
  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist

  if ( _linkstat == NULL ) return;

  _linkstat->get_neighbors(&neighbors);
  click_chatter("Have %d neigh",neighbors.size());

  notify_callback(5);
}

void DHTRoutingOmni::add_handlers()
{
}

#include <click/vector.cc>

template class Vector<DHTRoutingOmni::BufferedPacket>;

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTRoutingOmni)
