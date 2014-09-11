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
 * floodingtxscheduling.{cc,hh}
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

#include "floodingtxscheduling.hh"
#include "elements/brn/routing/broadcast/flooding/flooding.hh"

CLICK_DECLS

FloodingTxScheduling::FloodingTxScheduling():
  _me(NULL),
  _fhelper(NULL),
  _flooding_db(NULL),
  _dfl_retries(PASSIVE_ACK_DFL_MAX_RETRIES),
  _dfl_interval(PASSIVE_ACK_DFL_INTERVAL),
  _dfl_timeout(PASSIVE_ACK_DFL_TIMEOUT),
  _tx_scheduling(FLOODING_TXSCHEDULING_DEFAULT)
{
  BRNElement::init();
}

FloodingTxScheduling::~FloodingTxScheduling()
{
}

int
FloodingTxScheduling::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
      "FLOODINGDB", cpkP+cpkM, cpElement, &_flooding_db,
      "DEFAULTINTERVAL", cpkP, cpInteger, &_dfl_interval,
      "SCHEDULING", cpkP, cpInteger, &_tx_scheduling,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  if (FLOODING_TXSCHEDULING_LAST < _tx_scheduling) {
    BRN_ERROR("Unknown tx-scheduling! Use default.");
    _tx_scheduling = FLOODING_TXSCHEDULING_DEFAULT;
  }

  return 0;
}

int
FloodingTxScheduling::initialize(ErrorHandler *)
{
  click_brn_srandom();

  return 0;
}

int
FloodingTxScheduling::tx_delay(PassiveAckPacket *pap)
{
  int delay = 0;

  switch (_tx_scheduling) {
    case FLOODING_TXSCHEDULING_DEFAULT:
    case FLOODING_TXSCHEDULING_NEIGHBOURS_CNT:
      delay = tx_delay_cnt_neighbors_delay(pap);
      break;
    case FLOODING_TXSCHEDULING_MAX_DELAY:
      delay = tx_delay_max_delay(pap);
      break;
    case FLOODING_TXSCHEDULING_PRIO:
      delay = tx_delay_prio(pap);
      break;
  }

  return delay;
}

int
FloodingTxScheduling::tx_delay_prio(PassiveAckPacket *pap)
{
  /**
   * 1. Get all Neighbours
   * 2. get benefit of neighbours (looks like unicast)
   * 3. get probability of neighbours
   * 4. set priority depanding on the benefit and tx_probability (Metric?) : F(benefit,prob) = Prio
   * 5. Get time for prio depending on packetsize
   * */
  BroadcastNode *bcn = _flooding_db->get_broadcast_node(&(pap->_src));
  EtherAddress me = *_me->getMasterAddress();
  HashMap<EtherAddress, int> benefit_map;
  Vector<int> benefits;

  //get all neighbors
  CachedNeighborsMetricList* own_cnml = _fhelper->get_filtered_neighbors(me);

  int own_benefit = 0;
  int benefit = 0;

  int higher_prio = 0; //0 -> highest prio ..... bigger -> lower prio
  int same_prio = 0;

  BRN_ERROR("Neighbours: %d",own_cnml->_neighbors.size());
  //get benefit
  if ( own_cnml->_neighbors.size() > 0 ) {
    for ( int x = own_cnml->_neighbors.size()-1; x >= 0; x-- ) {
      int neighbors_prob = bcn->get_probability(pap->_bcast_id, &(own_cnml->_neighbors[x]));

      own_benefit += FloodingHelper::metric2pdr(own_cnml->get_metric(own_cnml->_neighbors[x])) * (100-neighbors_prob);

      //BRN_ERROR("own metric: %d", FloodingHelper::metric2pdr(own_cnml->get_metric(own_cnml->_neighbors[x])));

      CachedNeighborsMetricList* cnml = _fhelper->get_filtered_neighbors(own_cnml->_neighbors[x]);
      benefit = 0;

      /*
      BRN_ERROR("Neighbours Prob: %d own metric: %d b: %d", neighbors_prob, FloodingHelper::metric2pdr(own_cnml->get_metric(own_cnml->_neighbors[x])),
                                                      FloodingHelper::metric2pdr(own_cnml->get_metric(own_cnml->_neighbors[x])) * (100-neighbors_prob));
      */

      if ( neighbors_prob > 20 ) {
        for ( int n_i = cnml->_neighbors.size()-1; n_i >= 0; n_i--) {

          /*
          BRN_ERROR("for metric: %d p: %d", FloodingHelper::metric2pdr(cnml->get_metric(cnml->_neighbors[n_i])),
                                            bcn->get_probability(pap->_bcast_id, &(cnml->_neighbors[n_i])));
          */

          benefit += (neighbors_prob * FloodingHelper::metric2pdr(cnml->get_metric(cnml->_neighbors[n_i]))) * // P(tx neighbour to his n is successfull)
                    (100 - bcn->get_probability(pap->_bcast_id, &(cnml->_neighbors[n_i])));
        }
      }

      benefit /= 100;

      BRN_ERROR("Neighbour: %s Benefit: %d", own_cnml->_neighbors[x].unparse().c_str(), benefit);

      benefit_map.insert(own_cnml->_neighbors[x], benefit);
      benefits.push_back(benefit);
    }

    BRN_ERROR("Own benefit: %d",own_benefit);
    for ( int x = 0; x < benefits.size(); x++ ) {
      if ( benefits[x] > own_benefit ) higher_prio++; //node with more benefit, so my prio is less
      else if ( benefits[x] == own_benefit ) same_prio++; //node with same benefit
    }
  }

  int delay = (higher_prio * 2) + (click_random() % (same_prio+1));

  benefits.clear();
  benefit_map.clear();

  return delay;
}

int
FloodingTxScheduling::tx_delay_cnt_neighbors_delay(PassiveAckPacket *pap)
{
  /* depends on neighbours and last node*/
  int n = _fhelper->get_filtered_neighbors(*(_me->getMasterAddress()))->size();
  if ( n == 0 ) return (click_random() % _dfl_interval);

  int un = pap->_unfinished_neighbors.size();
  if ( n == un ) return (click_random() % _dfl_interval);

  /* more finished neighbors (relative) means mor waiting time */
  //int mod_time = (_dfl_interval * (n-un))/n;
  int mod_time = isqrt32((_dfl_interval * _dfl_interval * (n-un))/n);
  //int mod_time = (_dfl_interval * (n-un) * (n-un))/(n*n);

  if (mod_time == 0) return (click_random() % _dfl_interval);

  return (click_random() % mod_time);

  /* depends on nothing. Just like backoff */
  //return (click_random() % _dfl_interval);
}

int
FloodingTxScheduling::tx_delay_max_delay(PassiveAckPacket */*pap*/)
{
  /* depends on nothing. Just like backoff */
  return (click_random() % _dfl_interval);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
FloodingTxScheduling::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingTxScheduling)
