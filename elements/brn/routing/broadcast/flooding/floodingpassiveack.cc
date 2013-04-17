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
#include <click/etheraddress.hh>


#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "floodingpassiveack.hh"

CLICK_DECLS

FloodingPassiveAck::FloodingPassiveAck():
  _dfl_retries(0),
  _dfl_timeout(0),
  _retransmit_element(NULL),
  _flooding(NULL),
  _retransmit_broadcast(NULL)
{
  BRNElement::init();
}

FloodingPassiveAck::~FloodingPassiveAck()
{
  for ( int i = 0; i < p_queue.size(); i++ ) {
    PassiveAckPacket *p_next = p_queue[i];
    p_next->_p->kill();
    delete p_next;
  }
  
  p_queue.clear();
}

int
FloodingPassiveAck::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEFAULTRETRIES", cpkP, cpInteger, &_dfl_retries,
      "DEFAULTTIMEOUT", cpkP, cpInteger, &_dfl_timeout,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
FloodingPassiveAck::initialize(ErrorHandler *)
{
  return 0;
}

int
FloodingPassiveAck::packet_enqueue(Packet *p, EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries)
{
  if ( retries < 0 ) retries = _dfl_retries;

  PassiveAckPacket *pap = new PassiveAckPacket(p, src, bcast_id, passiveack, retries, _dfl_timeout);
  
  p_queue.push_back(pap);
    
  return 0;
}

void
FloodingPassiveAck::packet_dequeue(EtherAddress *src, uint16_t bcast_id)
{
  for ( int i = 0; i < p_queue.size(); i++ ) {
    PassiveAckPacket *p_next = p_queue[i];
    if ((p_next->_bcast_id == bcast_id) && (p_next->_src == *src)) {
      p_next->_p->kill();
      delete p_next;
      p_queue.erase(p_queue.begin() + i);
      break;
    }   
  }
}

FloodingPassiveAck::PassiveAckPacket *
FloodingPassiveAck::get_next_packet()
{
  Timestamp now = Timestamp::now();
  
  if ( p_queue.size() == 0 ) return NULL;

  PassiveAckPacket *pap = p_queue[0];
  int32_t next_time  = pap->time_left(now);
  
  for ( int i = 1; i < p_queue.size(); i++ ) {
    PassiveAckPacket *p_next = p_queue[i];
    if ( p_next->time_left(now) < next_time ) {
      next_time = p_next->time_left(now);
      pap = p_next;
    }
  }
  
  return pap;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

String
FloodingPassiveAck::stats()
{
  StringAccum sa;
  Timestamp now = Timestamp::now();

  sa << "<floodingpassiveack node=\"" << BRN_NODE_NAME << "\" flooding=\"" << (int)((_flooding!=NULL)?1:0);
  sa << "\" >\n\t<packetqueue count=\"" << p_queue.size() << "\" />\n";

  for ( int i = 0; i < p_queue.size(); i++ ) {
    PassiveAckPacket *p_next = p_queue[i];
    
    sa << "\t\t<packet src=\"" << p_next->_src.unparse() << "\" bcast_id=\"" << (uint32_t)p_next->_bcast_id;
    sa << "\" enque_time=\"" << p_next->_enqueue_time.unparse() << "\" time_left=\"" << p_next->time_left(now);
    sa << "\" />\n";
  }
  
  sa << "</floodingpassiveack>\n";

  return sa.take_string();
}

static String
read_stats_param(Element *e, void *)
{
  return ((FloodingPassiveAck *)e)->stats();
}

void
FloodingPassiveAck::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_stats_param, 0);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingPassiveAck)
