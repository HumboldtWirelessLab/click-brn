#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "brn2_dhtrouting_omniscient.hh"

#include "elements/brn/nblist.hh"
#include "elements/brn/brnlinkstat.hh"
#include "elements/brn/dht/dhtcommunication.hh"


CLICK_DECLS

DHTRoutingOmni::DHTRoutingOmni():
  _sendbuffer_timer(static_queue_timer_hook,this),
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
  _min_jitter = 100;                         //delay packets about a minimum of 100 ms
  _jitter = 1000;                            //delay packets up to 1 sec
  _simulator = 0;                            //for simulator
  _update_interval = 1000;                   //update interval -> 1 sec
  _min_dist = 100;                           //min. time distance between 2 packages
                                        //maybe this is only important for simulation
  if (cp_va_parse(conf, this, errh,
    cpOptional,
    cpKeywords,
    "LINKSTAT", cpElement, "LinkStat", &_linkstat,
    "UPDATEINT", cpInteger, "updateinterval", &_update_interval,
    "JITTER", cpInteger, "jitter", &_jitter,
    "MINJITTER", cpInteger, "minjitter", &_min_jitter,
    "SIMULATOR", cpInteger, "simulator", &_simulator,
    "DEBUG", cpInteger, "debug", &_debug,
    cpEnd) < 0)
      return -1;

  if (!_linkstat || !_linkstat->cast("BRNLinkStat"))
  {
    _linkstat = NULL;
    click_chatter("kein Linkstat");
  }

  if ( _min_jitter > _jitter ) _min_jitter = _jitter - 1;          //set min_jitter smaller than jitter

  return 0;
}

int DHTRoutingOmni::initialize(ErrorHandler *)
{
  _sendbuffer_timer.initialize(this);
  _sendbuffer_timer.schedule_after_msec( 1 + ( _min_jitter + ( random() % ( _jitter - _min_jitter ) ) ) );
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( _update_interval );

  return 0;
}
void DHTRoutingOmni::static_queue_timer_hook(Timer *, void *f)
{
  ((DHTRoutingOmni*)f)->queue_timer_hook();
}

void DHTRoutingOmni::set_lookup_timer()
{
  _lookup_timer.schedule_after_msec( _update_interval );
}

void DHTRoutingOmni::static_lookup_timer_hook(Timer *t, void *f)
{
  ((DHTRoutingOmni*)f)->nodeDetection();
  ((DHTRoutingOmni*)f)->set_lookup_timer();
}

void DHTRoutingOmni::push( int port, Packet *packet )
{
}

Packet *DHTRoutingOmni::createInfoPacket( uint8_t type )
{
  int i;
  WritablePacket *dht_packet = DHTPacketUtil::new_dht_packet();

  struct dht_packet_header *dht_header;
  uint8_t *dht_payload;

  struct dht_routing_header *routing_header;

  struct dht_node_info *_node_info;

  if ( _dhtnodes.size() > 128 )
  {
    click_chatter("DHT: sendNodeInfo: macht die Grenzen von 128 DHT-Knoten weg");
    return NULL;
  }
  else
  {
    DHTPacketUtil::set_header(dht_packet, DHT, DHT, 0, ROUTING , 0 );

    dht_packet = dht_packet->put( sizeof(struct dht_routing_header)
                                 + ( _dhtnodes.size() * sizeof(struct dht_node_info) ) ); //alle Ethernetadressen
    dht_header = (struct dht_packet_header *)dht_packet->data();

    dht_payload = (uint8_t *)dht_packet->data();
    routing_header = ( struct dht_routing_header * )&dht_payload[ sizeof(struct dht_packet_header) ];

    memcpy(dht_header->sender_hwa,_me._ether_addr.data(),6);
    routing_header->_type = type;
    routing_header->_num_send_nodes = _dhtnodes.size();
    routing_header->_num_dht_nodes = _dhtnodes.size();
     
    dht_payload = &dht_payload[ sizeof(struct dht_packet_header) + sizeof(struct dht_routing_header) ];
    _node_info = (struct dht_node_info *)dht_payload;

    for ( i = 0 ; i < _dhtnodes.size(); i++ )
    {
      _node_info[i]._hops = 2;
//      memcpy( _node_info[i]._ether_addr, _dhtnodes[i].ether_addr.data() , 6);
    }
    return (dht_packet);
  }
}

long diff_in_ms(timeval t1, timeval t2)
{
  long s, us, ms;

  while (t1.tv_usec < t2.tv_usec) {
    t1.tv_usec += 1000000;
    t1.tv_sec -= 1;
  }

  s = t1.tv_sec - t2.tv_sec;

  us = t1.tv_usec - t2.tv_usec;
  ms = s * 1000L + us / 1000L;

  return ms;
}

void DHTRoutingOmni::queue_timer_hook()
{
  struct timeval curr_time;
  click_gettimeofday(&curr_time);

  for ( int i = ( packet_queue.size() - 1 ) ; i >= 0; i--)
  {
    if ( diff_in_ms( packet_queue[i]._send_time, curr_time ) <= 0 )
    {
      Packet *out_packet = packet_queue[i]._p;
      packet_queue.erase(packet_queue.begin() + i);
      BRN_DEBUG("DHTRoutingOmni: Queue: Next Packet out!");
      output( 0 ).push( out_packet );
      break;
    }
  }

  int j = get_min_jitter_in_queue();
  if ( j < _min_dist ) j = _min_dist;

//    BRN_DEBUG("FalconDHT: Timer after %d ms", j );
 
  _sendbuffer_timer.schedule_after_msec(j);
}

int DHTRoutingOmni::get_min_jitter_in_queue()
{
  struct timeval _next_send;
  struct timeval _time_now;
  long next_jitter;

  if ( packet_queue.size() == 0 ) return( -1 );
  else
  {
    _next_send.tv_sec = packet_queue[0]._send_time.tv_sec;
    _next_send.tv_usec = packet_queue[0]._send_time.tv_usec;

    for ( int i = 1; i < packet_queue.size(); i++ )
    {
      if ( ( _next_send.tv_sec > packet_queue[i]._send_time.tv_sec ) ||
           ( ( _next_send.tv_sec == packet_queue[i]._send_time.tv_sec ) && (  _next_send.tv_usec > packet_queue[i]._send_time.tv_usec ) ) )
      {
        _next_send.tv_sec = packet_queue[i]._send_time.tv_sec;
        _next_send.tv_usec = packet_queue[i]._send_time.tv_usec;
      }
    }

    click_gettimeofday(&_time_now);

    next_jitter = diff_in_ms(_next_send, _time_now);

    if ( next_jitter <= 1 ) return( 1 );
    else return( next_jitter );   

  }
}

void
DHTRoutingOmni::sendNodeInfoToAll(uint8_t type)
{   
  Packet *dht_info_packet = createInfoPacket(type);

  uint16_t body_lenght = dht_info_packet->length();
  body_lenght = /*htons(*/body_lenght/*)*/;

  WritablePacket *dht_packet = dht_info_packet->push(20); //space for DMAC(6),SMAC(6),TYPE(2),BRNProtokoll(6)

  uint8_t *ether = dht_packet->data();
  uint16_t prot_type = 0x8680;

  memset(&ether[0],255,6);                               //set broadcastaddress 
  memcpy(&ether[6],_me._ether_addr.data(),6);            //i'am sender
  memcpy(&ether[12], &prot_type,2);                      //brn_protocol
  ether[14] = 7;                                         //dht
  ether[15] = 7;
  memcpy(&ether[16], &body_lenght,2);                    //lenght of dht_info
  ether[18] = 9;
  ether[19] = 0;

  click_ether *ether_he = (click_ether *)dht_packet->data();
  
  dht_packet->set_ether_header(ether_he);

  unsigned int j = (unsigned int ) ( _min_jitter +( random() % ( _jitter ) ) );
  packet_queue.push_back( BufferedPacket(dht_packet, j ));

  j = get_min_jitter_in_queue();

  _sendbuffer_timer.schedule_after_msec(j);

  //click_chatter("DHT (Node:%s) : Packet buffered",_me.s().c_str());
  
}


void DHTRoutingOmni::nodeDetection()
{
  int j,i,k,l;

  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist
  Vector<EtherAddress> new_neighbors;                   // new neighbors
  Vector<EtherAddress> lost_neighbors;                  // neighbors which got lost

  DHTnode *_dhtnode;

  if ( _linkstat == NULL ) return;

  _linkstat->get_neighbors(&neighbors);

  click_chatter("Have %d neigh",neighbors.size());

// new Nodes
  for ( i = 0 ; i < neighbors.size(); i++ )
  {
    for ( j = 0; j < my_neighbors.size(); j++ )
      if ( neighbors[i] == my_neighbors[j] ) break;

    if ( j == my_neighbors.size() )   // neighbor is new
    {
      new_neighbors.push_back( neighbors[i] );
      my_neighbors.push_back( neighbors[i] );
    }
  }

// removed Nodes

  for ( j = ( my_neighbors.size() - 1 ); j >= 0; j-- )      // search in all my_neighbors
  {
    i = 0;
    for ( ; i < neighbors.size(); i++ )                     // check wheter my neighbor is still my neighbor
      if ( my_neighbors[j] == neighbors[i] ) break;         // "i found him"

    if ( i == neighbors.size() )                            // my_neighbor is not neighbor anymore, so delete
    {
      lost_neighbors.push_back( my_neighbors[j] );         // save him is the lost-list

      _dhtnode = _dhtneighbors.get_dhtnode(&(my_neighbors[j]));  // check, whether he was a dht_neighbor

      if ( _dhtnode != NULL )                                    // he was a DHT_Neighbor
      {
        _dhtneighbors.erase_dhtnode(&(my_neighbors[j]));         // delete him in the dht neighbor-list
        _dhtnode = _dhtnodes.get_dhtnode(&(my_neighbors[j]));
        _dhtnode->_distance = 255;                                // mark him as "far away"

      }

      my_neighbors.erase(my_neighbors.begin() + j);        // rm neighbor, because is not my neighbor anymore

    }
  }
 
  if ( new_neighbors.size() > 0 )                           // send info to new neigh.; check wether he is DHT
  {
    click_chatter("Have %d new Neighbor",new_neighbors.size());
    sendNodeInfoToAll(ROUTE_REQUEST);                       // send infos to new_neighbours
  }
  if ( lost_neighbors.size() > 0 )                          // lost_neighbors: check wheter they are still alive
  {
    //BRN_DEBUG("DHT: DHT_Neighbor is lost! check, wether he is away or only not in range!");
    //requestForLostNodes(&lost_neighbors);
    click_chatter("");
  }

  lost_neighbors.clear();
  new_neighbors.clear();
  neighbors.clear();
}


int DHTRoutingOmni::set_notify_callback(void *info_func, void *info_obj)
{
  _info_func = info_func;
  _info_obj = info_obj;

  return 0;
}

void DHTRoutingOmni::add_handlers()
{
}

#include <click/vector.cc>

template class Vector<DHTRoutingOmni::BufferedPacket>;

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTRoutingOmni)
