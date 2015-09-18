#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn/standard/packetsendbuffer.hh"
#include "elements/brn/standard/brn_md5.hh"

#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/dht/protocol/dhtprotocol.hh"

#include "dhtrouting_nofudis.hh"
#include "dhtprotocol_nofudis.hh"


CLICK_DECLS

DHTRoutingNoFuDis::DHTRoutingNoFuDis():
  _lookup_timer(static_lookup_timer_hook,this),
  _packet_buffer_timer(static_packet_buffer_timer_hook,this)
{
  DHTRouting::init();
}

DHTRoutingNoFuDis::~DHTRoutingNoFuDis()
{
}

void *
DHTRoutingNoFuDis::cast(const char *name)
{
  if (strcmp(name, "DHTRoutingNoFuDis") == 0)
    return (DHTRoutingNoFuDis *) this;
  else if (strcmp(name, "DHTRouting") == 0)
         return (DHTRouting *) this;
       else
         return NULL;
}

int
DHTRoutingNoFuDis::configure(Vector<String> &conf, ErrorHandler *errh)
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

  return 0;
}

static int
tx_handler(void */*element*/, const EtherAddress*, char */*buffer*/, int/* size*/)
{
//  DHTRoutingNoFuDis *dhtro = (DHTRoutingNoFuDis*)element;
//  return lph->lpSendHandler(buffer, size);

  return 0;
}

static int
rx_handler(void */*element*/, EtherAddress*, char */*buffer*/, int/* size*/, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
//  DHTRoutingNoFuDis *dhtro = (DHTRoutingNoFuDis*)element;
//  return lph->lpReceiveHandler(buffer, size);*/

  return 0;
}

int
DHTRoutingNoFuDis::initialize(ErrorHandler *)
{
  if ( _linkstat )
    _linkstat->registerHandler(this,0,&tx_handler,&rx_handler);

  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( 5000 + _update_interval );
  _packet_buffer_timer.initialize(this);
  _packet_buffer_timer.schedule_after_msec( 10000 );

  return 0;
}

void
DHTRoutingNoFuDis::set_lookup_timer()
{
  _lookup_timer.schedule_after_msec( _update_interval );
}

void
DHTRoutingNoFuDis::static_lookup_timer_hook(Timer *t, void */*f*/)
{
  if ( t == NULL ) click_chatter("Time is NULL");
}

void
DHTRoutingNoFuDis::static_packet_buffer_timer_hook(Timer *t, void *f)
{
  DHTRoutingNoFuDis *dht;
  PacketSendBuffer::BufferedPacket *bpacket;
  int next_p;

  dht = (DHTRoutingNoFuDis*)f;

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
DHTRoutingNoFuDis::push( int /*port*/, Packet *packet )
{
  packet->kill();
}



/****************************************************************************************
****************************** N O D E   F O R   K E Y *+********************************
****************************************************************************************/
DHTnode *
DHTRoutingNoFuDis::get_responsibly_node_for_key(md5_byte_t */*key*/)
{
  return NULL;
}

DHTnode *
DHTRoutingNoFuDis::get_responsibly_node(md5_byte_t *key, int replica_number)
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
/****************************************************************************************
********************* N O D E T A B L E O P E R A T I O N *******************************
****************************************************************************************/


/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String 
DHTRoutingNoFuDis::routing_info(void)
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
  DHTRoutingNoFuDis *dht_omni = (DHTRoutingNoFuDis *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dht_omni->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingNoFuDis::add_handlers()
{
  DHTRouting::add_handlers();

  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolNoFuDis)
EXPORT_ELEMENT(DHTRoutingNoFuDis)
