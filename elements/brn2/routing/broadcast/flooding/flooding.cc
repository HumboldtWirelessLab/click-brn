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
 * flooding.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brn2_logger.hh"
#include "flooding.hh"
#include "floodingpolicy/floodingpolicy.hh"

CLICK_DECLS

Flooding::Flooding()
  :_sendbuffer_timer(this),
  _debug(Brn2Logger::DEFAULT),
  _flooding_src(0),
  _flooding_fwd(0)
{
}

Flooding::~Flooding()
{
}

int
Flooding::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int max_jitter;

  if (cp_va_kparse(conf, this, errh,
      "FLOODINGPOLICY", cpkP+cpkM , cpElement, &_flooding_policy,
      "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_my_ether_addr,
      "MINJITTER", cpkP+cpkM, cpInteger, /*"minimal Jitter",*/ &_min_jitter,
      "MAXJITTER", cpkP+cpkM, cpInteger, /*"maximal Jitter",*/ &max_jitter,
      "DIFFJITTER", cpkP+cpkM, cpInteger, /*"min Jitter between 2 Packets",*/ &_min_dist,
      cpEnd) < 0)
       return -1;

  _jitter = max_jitter - _min_jitter;
  return 0;
}

int
Flooding::initialize(ErrorHandler *)
{
  bcast_id = 0;

  unsigned int j = (unsigned int ) ( _min_jitter + ( click_random() % ( _jitter ) ) );
  click_chatter("BRNFlooding: Timer after %d ms", j );
  _sendbuffer_timer.initialize(this);
  _sendbuffer_timer.schedule_after_msec( j );
  return 0;
}

void
Flooding::push( int port, Packet *packet )
{
  click_chatter("Flooding: PUSH\n");
  struct click_brn_bcast *bcast_header;  /*{ uint16_t      bcast_id; hwaddr        dsr_dst; hwaddr        dsr_src; };*/
  uint8_t src_hwa[6];

  uint8_t broadcast[] = { 255,255,255,255,255,255 };

  if ( port == 0 )                       // kommt von arp oder so (Client)
  {
    click_chatter("Flooding: PUSH vom Client\n");

    uint8_t *packet_data = (uint8_t *)packet->data();
    memcpy(&src_hwa[0], &packet_data[6], 6 );

    packet->pull(6); //remove mac Broadcast-Address
    WritablePacket *new_packet = packet->push(sizeof(struct click_brn_bcast)); //add BroadcastHeader
    struct click_brn_bcast *bch = (struct click_brn_bcast*)new_packet->data();
    bcast_id++;
    bch->bcast_id = bcast_id;

    _flooding_policy->add_broadcast(&_my_ether_addr,(int)bcast_id);
    _flooding_src++;

    WritablePacket *out_packet = BRNProtocol::add_brn_header(new_packet, BRN_PORT_SIMPLEFLOODING, BRN_PORT_SIMPLEFLOODING);
    packet_data = (uint8_t *)out_packet->data();
    BRNPacketAnno::set_ether_anno(out_packet, _my_ether_addr, EtherAddress(broadcast), 0x8086);

    bcast_header = (struct click_brn_bcast *)&packet_data[sizeof(click_ether) + 6];

    EtherAddress new_eth = EtherAddress(src_hwa);
    click_chatter("Queue size:%d Hab %s:%d kommt vom Client",bcast_queue.size(),new_eth.unparse().c_str(),bcast_id);

    bcast_queue.push_back(BrnBroadcast(bcast_id, src_hwa));
    if ( bcast_queue.size() > SF_MAX_QUEUE_SIZE ) bcast_queue.erase( bcast_queue.begin() );

    unsigned int j = (unsigned int ) ( _min_jitter +( click_random() % ( _jitter ) ) );
    packet_queue.push_back( BufferedPacket(out_packet, j ));
  }

  if ( port == 1 )                                    // kommt von brn
  {
    click_chatter("Flooding: PUSH von BRN\n");

    uint8_t *packet_data = (uint8_t *)packet->data();
    struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)packet_data;

    EtherAddress src_eth = EtherAddress((uint8_t*)&packet_data[sizeof(struct click_brn_bcast)]);

    int i;
    for( i = 0; i < bcast_queue.size(); i++ )
     if ( ( bcast_queue[i].bcast_id == bcast_header->bcast_id ) && 
          ( memcmp( &bcast_queue[i].dsr_src[0], src_eth.data(), 6 ) == 0 ) ) break;

    bool is_known = (i != bcast_queue.size());

    if ( ! is_known ) {   //note and send to client only if this is the first time
      bcast_queue.push_back(BrnBroadcast( bcast_header->bcast_id, src_eth.data() ) );
      if ( bcast_queue.size() > SF_MAX_QUEUE_SIZE ) bcast_queue.erase( bcast_queue.begin() );

      /* Packete an den Client sofort raus , an andere Knoten in die Jitter-Queue */
      Packet *p_client = packet->clone();                 //packet for client
      p_client->pull(sizeof(struct click_brn_bcast));     //remove bcast_header
      WritablePacket *p_client_out = p_client->push(6);   //add space for bcast_addr
      memcpy(p_client_out->data(), broadcast, 6);         //set dest to bcast
      output( 0 ).push( p_client_out );                   // to clients (arp,...)
    }

    if ( _flooding_policy->do_forward(&src_eth, bcast_header->bcast_id, is_known) )
    {
      _flooding_fwd++;

      click_chatter("Queue size:%d Soll %s:%d forwarden",bcast_queue.size(), src_eth.unparse().c_str(),
                                                         bcast_header->bcast_id);

      WritablePacket *out_packet = BRNProtocol::add_brn_header(packet, BRN_PORT_SIMPLEFLOODING, BRN_PORT_SIMPLEFLOODING);
      BRNPacketAnno::set_ether_anno(out_packet, _my_ether_addr, EtherAddress(broadcast), 0x8086);

      unsigned int j = (unsigned int ) ( _min_jitter +( click_random() % ( _jitter ) ) );
      packet_queue.push_back( BufferedPacket( out_packet, j ) );
    } else {
      click_chatter("kenn ich schon: %s:%d",src_eth.unparse().c_str(), bcast_header->bcast_id);
      packet->kill();
    }
  }

  unsigned int j = get_min_jitter_in_queue();
  click_chatter("BRNFlooding: Timer after %d ms", j );

  _sendbuffer_timer.schedule_after_msec(j);
}

void
Flooding::run_timer(Timer *)
{
  queue_timer_hook();
}

void
Flooding::queue_timer_hook()
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

  _sendbuffer_timer.schedule_after_msec(j);
}

long
Flooding::diff_in_ms(timeval t1, timeval t2)
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
Flooding::get_min_jitter_in_queue()
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
  Flooding *fl = (Flooding *)e;

  return String(fl->_debug) + "\n";
}

static String
read_stat_param(Element *e, void *)
{
  Flooding *fl = (Flooding *)e;
  StringAccum sa;

  sa << "Source of Flooding: " << fl->_flooding_src;
  sa << "\nForward of Flooding: " << fl->_flooding_fwd << "\n";

  return sa.take_string();
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  Flooding *fl = (Flooding *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
Flooding::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
  add_read_handler("stat", read_stat_param, 0);
}

#include <click/vector.cc>
template class Vector<Flooding::BrnBroadcast>;
template class Vector<Flooding::BufferedPacket>;

CLICK_ENDDECLS
EXPORT_ELEMENT(Flooding)
