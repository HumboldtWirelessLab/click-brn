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
#include "elements/brn/routing/broadcast/flooding/flooding.hh"


CLICK_DECLS

FloodingPassiveAck::FloodingPassiveAck():
  _me(NULL),
  _retransmit_element(NULL),
  _flooding(NULL),
  _fhelper(NULL),
  _dfl_retries(1),
  _dfl_timeout(25),
  _time_tolerance(10),
  _retransmit_timer(this),
  _timer_is_scheduled(false),
  _queued_pkts(0),
  _dequeued_pkts(0),
  _retransmissions(0),
  _pre_removed_pkts(0),
  _already_queued_pkts(0),
  _retransmit_broadcast(NULL)
{
  BRNElement::init();
}

FloodingPassiveAck::~FloodingPassiveAck()
{
  for ( int i = 0; i < p_queue.size(); i++ ) {
    PassiveAckPacket *p_next = p_queue[i];
    delete p_next;
  }
  
  p_queue.clear();
}

int
FloodingPassiveAck::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
      "DEFAULTRETRIES", cpkP, cpInteger, &_dfl_retries,
      "DEFAULTTIMEOUT", cpkP, cpInteger, &_dfl_timeout,
      "TIMETOLERANCE", cpkP, cpInteger, &_time_tolerance,
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
  _timer_is_scheduled = false;
  //BRN_INFO("Scan queue");
  scan_packet_queue(_time_tolerance);
  //BRN_INFO("Set schedule");
  set_next_schedule();
  //BRN_INFO("run_timer done");
}

int
FloodingPassiveAck::packet_enqueue(Packet *p, EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries)
{
  if ( retries < 0 ) retries = _dfl_retries;
  
  PassiveAckPacket *pap = new PassiveAckPacket(NULL, src, bcast_id, passiveack, retries, _dfl_timeout);
  
  p_queue.push_back(pap);
  _queued_pkts++;
  
  _retransmit_broadcast(_retransmit_element, p, src, bcast_id );
  
  set_next_schedule();
    
  return 0;
}

void
FloodingPassiveAck::packet_dequeue(EtherAddress *src, uint16_t bcast_id)
{
  for ( int i = 0; i < p_queue.size(); i++ ) {
    PassiveAckPacket *p_next = p_queue[i];
    if ((p_next->_bcast_id == bcast_id) && (p_next->_src == *src)) {
      delete p_next;
      p_queue.erase(p_queue.begin() + i);
      _dequeued_pkts++;
      break;
    }
  }
}

void
FloodingPassiveAck::handle_feedback_packet(Packet *p, EtherAddress *src, uint16_t bcast_id, bool rejected)
{
  BRN_FATAL("Feedback: %s %d",src->unparse().c_str(), bcast_id);
  struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)(p->data());
  FloodingPassiveAck::PassiveAckPacket *pap = NULL;

  for ( int i = 0; i < p_queue.size(); i++ ) {
     pap = p_queue[i];
     if ((pap->_bcast_id == bcast_id) && (pap->_src == *src)) break;
  }
  
  assert(pap != NULL);
  /*if ( pap == NULL ) {
    p->kill();
    return;
  }*/
  
  pap->_p = p;
  _queued_pkts--;
  
  if ( (rejected && ((bcast_header->flags & BCAST_HEADER_FLAGS_REJECT_WITH_ASSIGN) != 0 )) ||
       ( pap->retries_left() == 0 ) ||
       packet_is_finished(pap)) {
    if ( pap->retries_left() != 0 ) _pre_removed_pkts++;
    packet_dequeue(src, bcast_id);
  }
}

FloodingPassiveAck::PassiveAckPacket *
FloodingPassiveAck::get_next_packet()
{
  if ( p_queue.size() == 0 ) return NULL;

  Timestamp now = Timestamp::now();

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
  
  if ( pap == NULL ) {
    _timer_is_scheduled = false;
    _retransmit_timer.unschedule();
  } else {
  
    Timestamp now = Timestamp::now();
    
    int32_t next_time  = pap->time_left(now);
    
    if ( next_time < 0 ) {  //TODO: what if this is zero?
      BRN_FATAL("Failure: flux capacitor! Back to the Future!: %d", next_time);
    }
    
    next_time = MAX(next_time, (int32_t)_time_tolerance); //time to next schedule at least 10 ms
    
    //BRN_DEBUG("Next schedule in %d ms",next_time);
    if ( _timer_is_scheduled ) {
      if ( (_time_next_schedule - now).msecval() <= next_time ) return;
      
      //BRN_DEBUG("Unschedule Timer");
      _retransmit_timer.unschedule();
    }
    
    _retransmit_timer.schedule_after_msec(next_time);
    _timer_is_scheduled = true;
    _time_next_schedule = now + Timestamp::make_msec(next_time);
  }
}

void
FloodingPassiveAck::scan_packet_queue(int32_t time_tolerance)
{  
  if ( p_queue.size() == 0 ) return;

  Timestamp now = Timestamp::now();

  for ( int i = p_queue.size() - 1; i >= 0; i-- ) {
    PassiveAckPacket *p_next = p_queue[i];
    if ( has_packet_in_queue(p_next) ) {
      BRN_FATAL("Packet (%s %d) is already in queue. Delay next retry",p_next->_src.unparse().c_str(),p_next->_bcast_id);
      _already_queued_pkts++;
      p_next->set_next_timeout();
      p_next->_already_queued_cnt++;
      continue; 
    }
    
    if ( p_next->time_left(now) <  time_tolerance) {
      
      //BRN_DEBUG("Packet %d Retries %d",i,p_next->retries_left());
      p_next->set_next_retry();

      /*int result = */
      _retransmit_broadcast(_retransmit_element, p_next->_p, &p_next->_src, p_next->_bcast_id );
      p_next->_p = NULL;
      _retransmissions++;
	
      //BRN_DEBUG("Late: Packet %d Retries %d",i,p_next->retries_left());
    }
  }
  //BRN_INFO("Scan done");
}

bool
FloodingPassiveAck::has_packet_in_queue(PassiveAckPacket *pap)
{
  return _flooding->unfinished_forward_attempts(&pap->_src, pap->_bcast_id) != 0;
}

bool
FloodingPassiveAck::packet_is_finished(PassiveAckPacket *pap)
{
  /*check retries*/
  if ( pap->retries_left() == 0 ) return true;
  else return false;
  
  /*check neighbours*/
  CachedNeighborsMetricList* cnml = _fhelper->get_filtered_neighbors(*(_me->getMasterAddress()));
  
  struct Flooding::BroadcastNode::flooding_last_node *last_nodes;
  uint32_t last_nodes_size;
  last_nodes = _flooding->get_last_nodes(&pap->_src, pap->_bcast_id, &last_nodes_size);
  
  BRN_DEBUG("For %s:%d i have %d neighbours", pap->_src.unparse().c_str(), pap->_bcast_id, last_nodes_size);
  
  int unfinished_neighbors = cnml->_neighbors.size();
  
  for ( int i = unfinished_neighbors-1; i >= 0; i--) {    //!! unfinished_neighbors will decreased in loop !!
    for ( uint32_t j = 0; j < last_nodes_size; j++) {
      if ( memcmp(cnml->_neighbors.data(), last_nodes[j].etheraddr, 6) == 0) {
        unfinished_neighbors--;
        break;
      }
    }  
  }
  
  return (unfinished_neighbors == 0);
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
  sa << "\" retries=\"" << _dfl_retries << "\" timeout=\"";
  sa << _dfl_timeout << "\" >\n\t<packetqueue count=\"" << p_queue.size();
  sa << "\" inserts=\"" << _queued_pkts << "\" deletes=\"" << _dequeued_pkts << "\" retransmissions=\"";
  sa << _retransmissions << "\" pre_deletes=\"" << _pre_removed_pkts;
  sa << "\" already_queued=\"" << _already_queued_pkts << "\" >\n";

  for ( int i = 0; i < p_queue.size(); i++ ) {
    PassiveAckPacket *p_next = p_queue[i];
    
    sa << "\t\t<packet src=\"" << p_next->_src.unparse() << "\" bcast_id=\"" << (uint32_t)p_next->_bcast_id;
    sa << "\" enque_time=\"" << p_next->_enqueue_time.unparse() << "\" time_left=\"" << p_next->time_left(now);
    sa << "\" time_out=\"" << p_next->_timeout << "\" retries=\"" << p_next->_retries;    
    sa << "\" already_queued=\"" << p_next->_already_queued_cnt << "\" />\n";
  }
  
  sa << "\t</packetqueue>\n</floodingpassiveack>\n";

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
