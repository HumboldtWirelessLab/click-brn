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
  _dfl_retries(1),
  _dfl_timeout(25),
  _retransmit_element(NULL),
  _flooding(NULL),
  _enable(true),
  _retransmit_timer(this),
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
      "ENABLE", cpkP, cpBool, &_enable,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
FloodingPassiveAck::initialize(ErrorHandler *)
{
  _retransmit_timer.initialize(this);
  return 0;
}

void
FloodingPassiveAck::run_timer(Timer *)
{
  scan_packet_queue(10);
  set_next_schedule();
}

int
FloodingPassiveAck::packet_enqueue(Packet *p, EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries)
{
  if ( !_enable ) return 0;
  
  if ( retries < 0 ) retries = _dfl_retries;

  PassiveAckPacket *pap = new PassiveAckPacket(p->clone(), src, bcast_id, passiveack, retries, _dfl_timeout);
  
  p_queue.push_back(pap);
  
  set_next_schedule();
    
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

void
FloodingPassiveAck::set_next_schedule()
{
  PassiveAckPacket *pap = get_next_packet();
  
  if ( pap == NULL ) _retransmit_timer.unschedule();
  
  Timestamp now = Timestamp::now();
  
  int32_t next_time  = pap->time_left(now);
  
  if ( next_time <= 0 ) {
    BRN_FATAL("Failure: flux capacitor! Back to the Future!");
    next_time = 10;
  }
  
  _retransmit_timer.reschedule_after_msec(next_time);
}

void
FloodingPassiveAck::scan_packet_queue(int32_t time_tolerance)
{  
  if ( p_queue.size() == 0 ) return;

  Timestamp now = Timestamp::now();

  for ( int i = p_queue.size() - 1; i >= 0; i-- ) {
    PassiveAckPacket *p_next = p_queue[i];
    
    if ( p_next->time_left(now) <  time_tolerance) {
      Packet *p = p_next->_p->clone();
      //if ( p_next->retries_left() != 0 ) p = p_next->_p->clone();
      /*int result = */_retransmit_broadcast(_retransmit_element, p, &p_next->_src, p_next->_bcast_id );
      p_next->set_next_retry();
      if ( p_next->retries_left() == 0 ) {
	p_next->_p->kill();
        //delete p_next;
        p_queue.erase(p_queue.begin() + i);
      }
    }
  }

  

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
  sa << "\" enabled=\"" << (int)((_enable)?1:0) << "\" >\n\t<packetqueue count=\"" << p_queue.size() << "\" />\n";

  for ( int i = 0; i < p_queue.size(); i++ ) {
    PassiveAckPacket *p_next = p_queue[i];
    
    sa << "\t\t<packet src=\"" << p_next->_src.unparse() << "\" bcast_id=\"" << (uint32_t)p_next->_bcast_id;
    sa << "\" enque_time=\"" << p_next->_enqueue_time.unparse() << "\" time_left=\"" << p_next->time_left(now);
    sa << "\" time_out=\"" << p_next->_timeout << "\" retries=\"" << p_next->_retries;    
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

static int
write_enable_param(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  FloodingPassiveAck *f = (FloodingPassiveAck *)e;
  String s = cp_uncomment(in_s);
  
  bool enable;
  if (!cp_bool(s, &enable))
    return errh->error("enable parameter must be boolean");

  f->enable(enable);
  
  return 0;
}     

void
FloodingPassiveAck::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_stats_param, 0);
  add_write_handler("enable", write_enable_param, 0);
  

}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingPassiveAck)
