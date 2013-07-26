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
  _enqueued_pkts(0),
  _queued_pkts(0),
  _dequeued_pkts(0),
  _retransmissions(0),
  _pre_removed_pkts(0),
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
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
FloodingPassiveAck::initialize(ErrorHandler *)
{
  click_srandom(_me->getMasterAddress()->hashcode());

  return 0;
}

int
FloodingPassiveAck::packet_enqueue(Packet *p, EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries)
{
  if ( retries < 0 ) retries = _dfl_retries;

  PassiveAckPacket *pap = new PassiveAckPacket(src, bcast_id, passiveack, retries);

  p_queue.push_back(pap);
  _queued_pkts++;
  _enqueued_pkts++;

  Timestamp next_tx = Timestamp::now() + Timestamp::make_msec(0, tx_delay(pap));
  pap->set_tx(next_tx);
  p->set_timestamp_anno(next_tx);

  _retransmit_broadcast(_retransmit_element, p, src, bcast_id );

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
  BRN_DEBUG("Feedback: %s %d",src->unparse().c_str(), bcast_id);

  struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)(p->data());
  FloodingPassiveAck::PassiveAckPacket *pap = NULL;

  for ( int i = 0; i < p_queue.size(); i++ ) {
     pap = p_queue[i];
     if ((pap->_bcast_id == bcast_id) && (pap->_src == *src)) break;
  }

  assert(pap != NULL);

  if ( (rejected && ((bcast_header->flags & BCAST_HEADER_FLAGS_REJECT_WITH_ASSIGN) != 0 )) ||
       ( pap->retries_left() == 0 ) ||
       packet_is_finished(pap)) {
    if ( pap->retries_left() != 0 ) _pre_removed_pkts++;
    _queued_pkts--;
    p->kill();
    packet_dequeue(src, bcast_id);
  } else {

    Timestamp next_tx = Timestamp::now() + Timestamp::make_msec(0, tx_delay(pap));
    pap->set_next_retry(next_tx);
    p->set_timestamp_anno(next_tx);

    _retransmit_broadcast(_retransmit_element, p, src, bcast_id );
    _retransmissions++;
  }
}

int
FloodingPassiveAck::count_unfinished_neighbors(PassiveAckPacket *pap)
{
  /*check neighbours*/
  CachedNeighborsMetricList* cnml = _fhelper->get_filtered_neighbors(*(_me->getMasterAddress()));

  struct Flooding::BroadcastNode::flooding_last_node *last_nodes;
  uint32_t last_nodes_size;
  last_nodes = _flooding->get_last_nodes(&pap->_src, pap->_bcast_id, &last_nodes_size);

  BRN_DEBUG("For %s:%d i have %d neighbours", pap->_src.unparse().c_str(), pap->_bcast_id, last_nodes_size);

  int unfinished_neighbors = cnml->_neighbors.size();

  for ( int i = unfinished_neighbors-1; i >= 0; i--) {    //!! unfinished_neighbors will decreased in loop !!
    for ( uint32_t j = 0; j < last_nodes_size; j++) {
      if ( memcmp(cnml->_neighbors[i].data(), last_nodes[j].etheraddr, 6) == 0) {
        unfinished_neighbors--;
        break;
      }
    }
  }

  return unfinished_neighbors;
}

bool
FloodingPassiveAck::packet_is_finished(PassiveAckPacket *pap)
{
  /*check retries*/
  if ( pap->retries_left() == 0 ) return true;

  return (count_unfinished_neighbors(pap) == 0);
}

int
FloodingPassiveAck::tx_delay(PassiveAckPacket *pap)
{
  if ( _flooding->me_src(&(pap->_src), pap->_bcast_id) && (pap->_retries == 0) ) return 0; //first

  /* depends on neighbours and last node*/
  int n = _fhelper->get_filtered_neighbors(*(_me->getMasterAddress()))->size();
  if ( n == 0 ) return (click_random() % _dfl_timeout);

  int un = count_unfinished_neighbors(pap);
  if ( n == un ) return (click_random() % _dfl_timeout);

  int mod_time = (_dfl_timeout * (n-un))/n;
  if (mod_time == 0) return (click_random() % _dfl_timeout);

  return (click_random() % mod_time);

  /* depends on nothing. Just like backoff */
  //return (click_random() % _dfl_timeout);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

String
FloodingPassiveAck::stats()
{
  StringAccum sa;

  sa << "<floodingpassiveack node=\"" << BRN_NODE_NAME << "\" flooding=\"" << (int)((_flooding!=NULL)?1:0);
  sa << "\" retries=\"" << _dfl_retries << "\" timeout=\"";
  sa << _dfl_timeout << "\" >\n\t<packetqueue count=\"" << p_queue.size();
  sa << "\" inserts=\"" << _enqueued_pkts << "\" deletes=\"" << _dequeued_pkts;
  sa << "\" queued=\"" << _queued_pkts;
  sa << "\" retransmissions=\"" << _retransmissions << "\" pre_deletes=\"" << _pre_removed_pkts << "\" >\n";

  for ( int i = 0; i < p_queue.size(); i++ ) {
    PassiveAckPacket *p_next = p_queue[i];

    sa << "\t\t<packet src=\"" << p_next->_src.unparse() << "\" bcast_id=\"" << (uint32_t)p_next->_bcast_id;
    sa << "\" enque_time=\"" << p_next->_enqueue_time.unparse() << "\" retries=\"" << p_next->_retries << "\" />\n";
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
