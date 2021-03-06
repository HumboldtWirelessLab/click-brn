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

#ifndef FLOODINGTXSCHEDULINGELEMENT_HH
#define FLOODINGTXSCHEDULINGELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>
#include <click/timer.hh>
#include <click/hashtable.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

#include "elements/brn/routing/broadcast/flooding/flooding_db.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_helper.hh"
#include "elements/brn/routing/broadcast/flooding/passiveack/passiveackpacketinfo.hh"

CLICK_DECLS

#define FLOODING_TXSCHEDULING_DEFAULT             0
#define FLOODING_TXSCHEDULING_NEIGHBOURS_CNT      1
#define FLOODING_TXSCHEDULING_MAX_DELAY           2
#define FLOODING_TXSCHEDULING_PRIO                3
#define FLOODING_TXSCHEDULING_ACTIVE_PASSIVE_PRIO 4

#define FLOODING_TXSCHEDULING_LAST                4


/*
 * =c
 * FloodingTxScheduling()
 * =s
 *
 * =d
 */

 /*
  * Flooding (oder ein andere element) 
  * 
  * 
  * 
  * 
  * 
  */

typedef HashTable<EtherAddress, uint32_t> BenefitMap;
typedef BenefitMap::const_iterator BenefitMapIter;

class FloodingTxScheduling : public BRNElement {

 public:
  //
  //methods
  //
  FloodingTxScheduling();
  ~FloodingTxScheduling();

  const char *class_name() const  { return "FloodingTxScheduling"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);

  void add_handlers();

 private:

  BRN2NodeIdentity *_me;

public:
  FloodingHelper *_fhelper;
  FloodingDB *_flooding_db;

  uint32_t _dfl_retries;
  uint32_t _dfl_interval;
  uint32_t _dfl_timeout;

  uint32_t _tx_scheduling;

  int tx_delay(PassiveAckPacket *pap);
  int tx_delay_prio(PassiveAckPacket *pap, bool active_passive_prio = false);
  int tx_delay_cnt_neighbors_delay(PassiveAckPacket *pap);
  int tx_delay_max_delay(PassiveAckPacket *pap);

};

CLICK_ENDDECLS
#endif
