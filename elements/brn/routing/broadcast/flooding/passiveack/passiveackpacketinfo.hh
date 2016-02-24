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

#ifndef FLOODINGPASSIVEACKPACKETINFO_HH
#define FLOODINGPASSIVEACKPACKETINFO_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>
#include <click/timer.hh>

CLICK_DECLS
class PassiveAckPacket
{
 public:
  EtherAddress _src;
  uint16_t _bcast_id;
  Vector<EtherAddress> _passiveack;

  int16_t _max_retries;
  uint16_t _retries;

  uint16_t _cnt_finished_passiveack_nodes;

  Timestamp _enqueue_time;
  Timestamp _last_tx;

  /* Some stats, which can be used, whether a small ack (floodingacknowledgment) is better than the full paket.
    *
    */
  Vector<EtherAddress> _unfinished_neighbors;
  Vector<int> _unfinished_neighbors_rx_prob;

  PassiveAckPacket(EtherAddress *src, uint16_t bcast_id, Vector<EtherAddress> *passiveack, int16_t retries): _src(src->data()), _bcast_id(bcast_id), _max_retries(retries),
                                                                                                             _retries(0), _cnt_finished_passiveack_nodes(0)
  {
    if ( passiveack != NULL )
      for ( int i = 0; i < passiveack->size(); i++) _passiveack.push_back((*passiveack)[i]);

    _last_tx = _enqueue_time = Timestamp::now();

    _unfinished_neighbors.clear();
    _unfinished_neighbors_rx_prob.clear();
  }

  ~PassiveAckPacket() {
    _passiveack.clear();
    _unfinished_neighbors.clear();
    _unfinished_neighbors_rx_prob.clear();
  }

  void set_tx(Timestamp tx_time) {
    _last_tx = tx_time;
  }

  inline void inc_max_retries() { _max_retries++; }; //use for tx_abort (is an net_layer transmission but maybe no mac-layer tx)

  void set_next_retry(Timestamp tx_time) {
    _retries++;
    _last_tx = tx_time;
  }

  inline int32_t tx_duration(Timestamp now) {
    return (now - _last_tx).msecval();
  }

  inline uint32_t retries_left() {
    return _max_retries - _retries;
  }

  bool tx_timeout(Timestamp &now, int timeout ) {
    return ((now - _enqueue_time).msecval() > timeout );
  }

};

CLICK_ENDDECLS
#endif
