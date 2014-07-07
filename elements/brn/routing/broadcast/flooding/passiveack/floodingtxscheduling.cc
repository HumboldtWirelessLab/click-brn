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
  _dfl_timeout(PASSIVE_ACK_DFL_TIMEOUT)
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
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

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
  return tx_delay_cnt_neighbors_delay(pap);
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
  Vector<EtherAddress> neighbours;

  (void)pap;

  return 0;
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
