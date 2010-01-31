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

#include "elements/brn2/standard/packetsendbuffer.hh"
#include "elements/brn2/standard/md5.h"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

#include "dhtrouting_dart.hh"
#include "dhtprotocol_dart.hh"

//#include "elements/brn/routing/nblist.hh"

CLICK_DECLS

DHTRoutingDart::DHTRoutingDart():
  _lookup_timer(static_lookup_timer_hook,this),
  _packet_buffer_timer(static_packet_buffer_timer_hook,this)
{
}

DHTRoutingDart::~DHTRoutingDart()
{
}

void *
DHTRoutingDart::cast(const char *name)
{
  if (strcmp(name, "DHTRoutingDart") == 0)
    return (DHTRoutingDart *) this;
  else if (strcmp(name, "DHTRouting") == 0)
         return (DHTRouting *) this;
       else
         return NULL;
}

int
DHTRoutingDart::configure(Vector<String> &conf, ErrorHandler *errh)
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
    click_chatter("kein Linkstat");
  }

  _me = new DHTnode(_my_ether_addr);
  _me->_status = STATUS_OK;
  _me->_neighbor = true;
  _dhtnodes.add_dhtnode(_me);

  return 0;
}

int
DHTRoutingDart::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( 5000 + _update_interval );
  _packet_buffer_timer.initialize(this);
  _packet_buffer_timer.schedule_after_msec( 10000 );

  return 0;
}

void
DHTRoutingDart::set_lookup_timer()
{
  _lookup_timer.schedule_after_msec( _update_interval );
}

void
DHTRoutingDart::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((DHTRoutingDart*)f)->set_lookup_timer();
}

void
DHTRoutingDart::static_packet_buffer_timer_hook(Timer *t, void *f)
{
  DHTRoutingDart *dht;
  PacketSendBuffer::BufferedPacket *bpacket;
  int next_p;

  dht = (DHTRoutingDart*)f;

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
DHTRoutingDart::push( int /*port*/, Packet *packet )
{
  packet->kill();
}



/****************************************************************************************
****************************** N O D E   F O R   K E Y *+********************************
****************************************************************************************/
DHTnode *
DHTRoutingDart::get_responsibly_node(md5_byte_t */*key*/)
{
  return NULL;
}

/****************************************************************************************
********************* N O D E T A B L E O P E R A T I O N *******************************
****************************************************************************************/

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String 
DHTRoutingDart::routing_info(void)
{
  StringAccum sa;

  sa << "Routing Info ( Node: " << _me->_ether_addr.unparse() << " )\n";

  return sa.take_string();
}

enum {
  H_ROUTING_INFO
};

static String
read_param(Element *e, void *thunk)
{
  DHTRoutingDart *dht_omni = (DHTRoutingDart *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_omni->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingDart::add_handlers()
{
  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolDart)
EXPORT_ELEMENT(DHTRoutingDart)
