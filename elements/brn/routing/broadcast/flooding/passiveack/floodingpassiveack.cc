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
  _fhelper(NULL),
  _flooding_db(NULL),
  _dfl_retries(PASSIVE_ACK_DFL_MAX_RETRIES),
  _dfl_timeout(PASSIVE_ACK_DFL_TIMEOUT),
  _cntbased_min_neighbors_for_abort(0),
  _abort_on_finished(true),
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
      "FLOODINGDB", cpkP+cpkM, cpElement, &_flooding_db,
      "FLOODINGSCHEDULING", cpkP+cpkM, cpElement, &_flooding_scheduling,
      "DEFAULTRETRIES", cpkP, cpInteger, &_dfl_retries,
      "DEFAULTTIMEOUT", cpkP, cpInteger, &_dfl_timeout,
      "CNTNB2ABORT", cpkP, cpInteger, &_cntbased_min_neighbors_for_abort,
      "ABORTONFINISHED", cpkP, cpBool, &_abort_on_finished,
      //"RETRIESPERNODE", cpkP, cpBool, &_retries_per_node,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
FloodingPassiveAck::initialize(ErrorHandler *)
{
  click_brn_srandom();

  return 0;
}

int
FloodingPassiveAck::packet_enqueue(Packet *p, EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries)
{
  if ( retries < 0 ) retries = _dfl_retries;

  //check whether paket i enqued again:this would indicate an error in flooding or flooding policy
  assert(NULL == get_pap(src, bcast_id));

  //no passiv ack nodes are given so take (filtered) neighbours
  if ( passiveack->size() == 0 ) {
    CachedNeighborsMetricList* cnml = _fhelper->get_filtered_neighbors(*(_me->getMasterAddress()));
    passiveack = &(cnml->_neighbors);
  }

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
FloodingPassiveAck::handle_feedback_packet(Packet *p, EtherAddress *src, uint16_t bcast_id, bool rejected, bool abort, uint8_t /*no_transmissions*/)
{
  BRN_DEBUG("Feedback/Abort: %s %d",src->unparse().c_str(), bcast_id);

  struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)(p->data());
  PassiveAckPacket *pap = NULL;

  for ( int i = 0; i < p_queue.size(); i++ ) {
     pap = p_queue[i];
     if ((pap->_bcast_id == bcast_id) && (pap->_src == *src)) break;
  }

  assert(pap != NULL);

  Timestamp now = Timestamp::now();

  //TODO: is it a net retries if i sent the packet one time or little bit more ??
  // if abort, then inc max retries to have one more retry (abort once)
  if (abort /*&& (no_transmissions == 0)*/) pap->inc_max_retries();

  set_unfinished_neighbors(pap);

  if ( (rejected && ((bcast_header->flags & BCAST_HEADER_FLAGS_REJECT_WITH_ASSIGN) == 0 )) ||  //low layer said: "it's done!!"
        packet_is_finished(pap) ||                                                             //or packet is finished (all passiv ack nodes or cnt based
        pap->tx_timeout(now, _dfl_timeout)) {                                                  //or timeout

    BRN_DEBUG("Finished with %d retries left! Reson: %d %d %d (reject pack-fin timeout)",pap->retries_left(), 
                (rejected && ((bcast_header->flags & BCAST_HEADER_FLAGS_REJECT_WITH_ASSIGN) == 0 ))?1:0,
                packet_is_finished(pap)?1:0, pap->tx_timeout(now, _dfl_timeout)?1:0);
    if ( pap->retries_left() != 0 ) _pre_removed_pkts++;

    _queued_pkts--;

    p->kill();
    packet_dequeue(src, bcast_id);

  } else {

    Timestamp next_tx = now + Timestamp::make_msec(0, tx_delay(pap));
    pap->set_next_retry(next_tx);
    p->set_timestamp_anno(next_tx);

    _retransmit_broadcast(_retransmit_element, p, src, bcast_id );
    _retransmissions++;
  }
}

//TODO: Whats that for??
void
FloodingPassiveAck::update_flooding_info(Packet *p, EtherAddress &src, uint16_t bcast_id)
{
  (void)p;
  (void)src;
  (void)bcast_id;

  return;
}

int
FloodingPassiveAck::set_unfinished_neighbors(PassiveAckPacket *pap)
{
  struct BroadcastNode::flooding_node_info *last_nodes;
  uint32_t last_nodes_size, j;
  last_nodes = _flooding_db->get_node_infos(&pap->_src, pap->_bcast_id, &last_nodes_size);

  BRN_DEBUG("For %s:%d i have %d nodes", pap->_src.unparse().c_str(), pap->_bcast_id, last_nodes_size);

  pap->_unfinished_neighbors.clear();
  pap->_unfinished_neighbors_rx_prob.clear();

  BroadcastNode *bcn = _flooding_db->get_broadcast_node(&(pap->_src));

  uint32_t no_rx_nodes = 0;

  for ( int i = pap->_passiveack.size()-1; i >= 0; i--) {    //!! unfinished_neighbors will decreased in loop !!

    BRN_DEBUG("PACK: check addr: %s",pap->_passiveack[i].unparse().c_str());

    for ( j = 0; j < last_nodes_size; j++) {
      if ((last_nodes[j].flags & FLOODING_NODE_INFO_FLAGS_FINISHED) == 0) continue; //it's not finished, so it doesn't matter
                                                                                     //whether it is the searched node

      //it's finished! now check whether it is the right node
      //break if node is found
      if ( memcmp(pap->_passiveack[i].data(), last_nodes[j].etheraddr, 6) == 0) {

        //node is finished and a passiveack node
        no_rx_nodes++;
        break;
      }
    }

    if ( j == last_nodes_size ) { //passiv_ack_node not found in last node, so its unfinished
      pap->_unfinished_neighbors.push_back(pap->_passiveack[i]);
      pap->_unfinished_neighbors_rx_prob.push_back(bcn->get_probability(pap->_bcast_id, &(pap->_passiveack[i])));
    }

  }

  pap->_cnt_finished_passiveack_nodes = no_rx_nodes;

  return pap->_unfinished_neighbors.size();
}


int
FloodingPassiveAck::count_unfinished_neighbors(PassiveAckPacket *pap)
{
  return pap->_unfinished_neighbors.size();
}

bool
FloodingPassiveAck::packet_is_finished(PassiveAckPacket *pap)
{
  /*check retries*/
  if ( pap->retries_left() == 0 ) return true;

  bool cnt_based_abort = ((_cntbased_min_neighbors_for_abort > 0) && (_cntbased_min_neighbors_for_abort <= pap->_cnt_finished_passiveack_nodes ));

  BRN_DEBUG("Finished?: Retries left: %d cnt_based_abort: %d abort_on_finished: %d count_finihsed_n: %d",
                        pap->retries_left(), cnt_based_abort?1:0,
                        _abort_on_finished,count_unfinished_neighbors(pap));

  return (_abort_on_finished && ((count_unfinished_neighbors(pap) == 0) || cnt_based_abort));
}

int
FloodingPassiveAck::tx_delay(PassiveAckPacket *pap)
{
  if ( _flooding_db->me_src(&(pap->_src), pap->_bcast_id) && (pap->_retries == 0) ) return 0; //first

  return _flooding_scheduling->tx_delay(pap);
}

PassiveAckPacket*
FloodingPassiveAck::get_pap(EtherAddress *src, uint16_t bcast_id)
{
  PassiveAckPacket *pap = NULL;

  for ( int i = 0; i < p_queue.size(); i++ ) {
     pap = p_queue[i];
     if ((pap->_bcast_id == bcast_id) && (pap->_src == *src)) return pap;
  }
  return NULL;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

String
FloodingPassiveAck::stats()
{
  StringAccum sa;

  sa << "<floodingpassiveack node=\"" << BRN_NODE_NAME << "\" flooding_db=\"" << (int)((_flooding_db!=NULL)?1:0);
  sa << "\" retries=\"" << _dfl_retries << "\" >\n\t<packetqueue count=\"" << p_queue.size();
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
