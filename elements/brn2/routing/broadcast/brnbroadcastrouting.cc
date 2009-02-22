/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * BrnBroadcastRouting.{cc,hh}
 */

#include <click/config.h>
#include "elements/brn/common.hh"

#include "brnbroadcastrouting.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

CLICK_DECLS

BrnBroadcastRouting::BrnBroadcastRouting()
  :_sendbuffer_timer(this),
  _debug(BrnLogger::DEFAULT)
{
}

BrnBroadcastRouting::~BrnBroadcastRouting()
{
}

int
BrnBroadcastRouting::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int max_jitter;

  if (cp_va_kparse(conf, this, errh,
      "MINJITTER", cpkP+cpkM, cpInteger, /*"minimal Jitter",*/ &_min_jitter,
      "MAXJITTER", cpkP+cpkM, cpInteger, /*"maximal Jitter",*/ &max_jitter,
      "DIFFJITTER", cpkP+cpkM, cpInteger, /*"min Jitter between 2 Packets",*/ &_min_dist,
      cpEnd) < 0)
       return -1;

  _jitter = max_jitter - _min_jitter;
  return 0;
}

int
BrnBroadcastRouting::initialize(ErrorHandler *)
{
  bcast_id = 0;

  unsigned int j = (unsigned int ) ( _min_jitter + ( click_random() % ( _jitter ) ) );
  click_chatter("BRNFlooding: Timer after %d ms", j );
  _sendbuffer_timer.initialize(this);
  _sendbuffer_timer.schedule_after_msec( j );
  return 0;
}

void
BrnBroadcastRouting::push( int port, Packet *packet )
{
  click_chatter("BrnBroadcastRouting: PUSH\n");
  struct click_brn_bcast *bcast_header;  /*{ uint16_t      bcast_id; hwaddr        dsr_dst; hwaddr        dsr_src; };*/
  uint8_t src_hwa[6];

  if ( port == 0 )                       // kommt von arp oder so (Client)
  { 
    click_chatter("BrnBroadcastRouting: PUSH vom Client\n");

    uint16_t body_lenght = packet->length();
    body_lenght = htons(body_lenght);

    uint8_t *packet_data = (uint8_t *)packet->data();
    memcpy(&src_hwa[0], &packet_data[6], 6 );

    WritablePacket *out_packet = packet->push( sizeof(click_ether) + 6 + 14 ); 
    packet_data = (uint8_t *)out_packet->data();

    packet_data[sizeof(click_ether) + 0] = BRN_PORT_BCAST;
    packet_data[sizeof(click_ether) + 1] = 4;
    memcpy(&packet_data[sizeof(click_ether) + 2], &body_lenght,2);
    packet_data[sizeof(click_ether) + 4] = 9;
    packet_data[sizeof(click_ether) + 5] = 0;

    click_ether *ether = (click_ether *)out_packet->data();

    uint8_t broadcast[] = { 255,255,255,255,255,255 };
    memcpy(ether->ether_dhost, &broadcast[0] , 6 );
    memcpy(ether->ether_shost, &src_hwa[0] , 6 );
    ether->ether_type = 0x8680; /*0*/

    out_packet->set_ether_header(ether);

    bcast_header = (struct click_brn_bcast *)&packet_data[sizeof(click_ether) + 6];

    bcast_id++;
    bcast_header->bcast_id = bcast_id;
    memcpy(&(bcast_header->dsr_src), &src_hwa[0], 6 );

    EtherAddress new_eth = EtherAddress((uint8_t*)&bcast_header->dsr_src);
    int new_id = bcast_header->bcast_id;
    click_chatter("Queue size:%d Hab %s:%d kommt vom Client",bcast_queue.size(),new_eth.unparse().c_str(),new_id);

    bcast_queue.push_back(BrnBroadcast( bcast_id, &src_hwa[0] ) );

    if ( bcast_queue.size() > MAX_QUEUE_SIZE ) bcast_queue.erase( bcast_queue.begin() );

    /* Packete an den Client sofort raus , an andere Knoten in die Jitter-Queue */  
    Packet *p_client = packet->clone();
    unsigned int j = (unsigned int ) ( _min_jitter +( click_random() % ( _jitter ) ) );
    packet_queue.push_back( BufferedPacket(out_packet, j ));

    output( 0 ).push( p_client );  // to clients (arp,...)
  }

  if ( port == 1 )                                    // kommt von brn
  {
    click_chatter("BrnBroadcastRouting: PUSH von BRN\n");

    uint8_t *packet_data = (uint8_t *)packet->data();
    struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)&packet_data[sizeof(click_ether) + 6];

    EtherAddress new_eth = EtherAddress((uint8_t*)&bcast_header->dsr_src);
    int new_id = bcast_header->bcast_id;

    int i;
    for( i = 0; i < bcast_queue.size(); i++ )
     if ( ( bcast_queue[i].bcast_id == bcast_header->bcast_id ) && 
        ( memcmp( &bcast_queue[i].dsr_src[0], &bcast_header->dsr_src, 6 ) == 0 ) ) break;

    if ( i == bcast_queue.size() ) // paket noch nie gesehen
    {
      click_chatter("Queue size:%d Hab %s:%d noch nie gesehen",bcast_queue.size(),new_eth.unparse().c_str(),new_id);
      bcast_queue.push_back(BrnBroadcast( bcast_header->bcast_id, (uint8_t*)&bcast_header->dsr_src ) );
      if ( bcast_queue.size() > MAX_QUEUE_SIZE ) bcast_queue.erase( bcast_queue.begin() );

      /* Packete an den Client sofort raus , an andere Knoten in die Jitter-Queue */  
      Packet *p_client = packet->clone();
      unsigned int j = (unsigned int ) ( _min_jitter +( click_random() % ( _jitter ) ) );
      packet_queue.push_back( BufferedPacket( packet, j ) );

      output( 0 ).push( p_client );  // to clients (arp,...)
    }
    else
      packet->kill();
  }

  unsigned int j = get_min_jitter_in_queue();
  click_chatter("BRNFlooding: Timer after %d ms", j );
 
  _sendbuffer_timer.schedule_after_msec(j);
}

void
BrnBroadcastRouting::run_timer(Timer *)
{
  queue_timer_hook();
}

void
BrnBroadcastRouting::queue_timer_hook()
{
  struct timeval curr_time;
  curr_time = Timestamp::now().timeval();

  for ( int i = 0; i < packet_queue.size(); i++)
  {
    if ( diff_in_ms( packet_queue[i]._send_time, curr_time ) <= 0 )
    {
      Packet *out_packet = packet_queue[i]._p;
      packet_queue.erase(packet_queue.begin() + i);
      output( 1 ).push( out_packet );
      break;
    }
  }

  unsigned int j = get_min_jitter_in_queue();
  if ( j < (unsigned)_min_dist ) j = _min_dist;
  click_chatter("BRNFlooding: Timer after %d ms", j );
 
  _sendbuffer_timer.schedule_after_msec(j);
}

long
BrnBroadcastRouting::diff_in_ms(timeval t1, timeval t2)
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

unsigned int
BrnBroadcastRouting::get_min_jitter_in_queue()
{
  struct timeval _next_send;
  struct timeval _time_now;
  long next_jitter;

  if ( packet_queue.size() == 0 ) return( 1000 );
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

    _time_now = Timestamp::now().timeval();

    next_jitter = diff_in_ms(_next_send, _time_now);

    if ( next_jitter <= 1 ) return( 1 );
    else return( next_jitter );
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BrnBroadcastRouting *fl = (BrnBroadcastRouting *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BrnBroadcastRouting *fl = (BrnBroadcastRouting *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
BrnBroadcastRouting::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

#include <click/vector.cc>
template class Vector<BrnBroadcastRouting::BrnBroadcast>;
template class Vector<BrnBroadcastRouting::BufferedPacket>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnBroadcastRouting)
