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
 * AlarmingRetransmit.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "alarmingretransmit.hh"
#include "alarmingstate.hh"
#include "alarmingprotocol.hh"

CLICK_DECLS

AlarmingRetransmit::AlarmingRetransmit():
  _timer(this),
  _alarm_active(false),
  _retransmit_time(ALARMING_DEFAULT_RETRANMIT_TIME)
{
  BRNElement::init();
}

AlarmingRetransmit::~AlarmingRetransmit()
{
}

int
AlarmingRetransmit::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM, cpElement, &_nodeid,
      "ALARMINGSTATE", cpkP+cpkM , cpElement, &_as,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
AlarmingRetransmit::initialize(ErrorHandler *)
{
  click_random_srandom();

  _timer.initialize(this);
  return 0;
}

void
AlarmingRetransmit::run_timer(Timer */*t*/)
{
//  click_chatter("Timer");
  if ( _alarm_active ) {
    BRN_INFO("Start Retransmit");
    retransmit_alarm();
  }
}


void
AlarmingRetransmit::retransmit_active(bool active)
{
  if ( active == _alarm_active ) return;

  _alarm_active = active;

  if ( _alarm_active ) {
    retransmit_alarm();
  } else {
    _timer.clear();   /* deactivate timer */

  }
}

void
AlarmingRetransmit::retransmit_alarm()
{
  WritablePacket *p = NULL;

  int send_packets = 0;

  _as->update_neighbours();

  Vector<int> alarm_types;
  Vector<AlarmingState::AlarmNode*> nodes;

//  click_chatter("get inclomplete types");
  _as->get_incomlete_forward_types(_as->_min_neighbour_fraction, &alarm_types);

  for ( int at_i = (alarm_types.size()-1); at_i >= 0; at_i-- ) {
    int alarm = alarm_types[at_i];

//    click_chatter("get node for type %d",alarm);
    _as->get_incomlete_forward_nodes(_as->_min_neighbour_fraction, _as->_retry_limit,  _as->_hop_limit, alarm, &nodes);

    for ( int an_i = (nodes.size()-1); an_i >= 0; an_i-- ) {
      AlarmingState::AlarmNode *an = nodes[an_i];

      for ( int ai_i = an->_info.size() - 1; ai_i >= 0; ai_i-- ) {
        AlarmingState::AlarmInfo *ai = &(an->_info[ai_i]);

        if ( p == NULL ) p = AlarmingProtocol::new_alarming_packet(alarm);

        p = AlarmingProtocol::add_node(p, &(an->_ea), START_TTL + ai->_hops, ai->_id);
        ai->_retries++;
      }
    }

    nodes.clear();

    if ( p != NULL ) {
 //     click_chatter("Retransmit");

      WritablePacket *p_out = BRNProtocol::add_brn_header(p, BRN_PORT_ALARMINGPROTOCOL, BRN_PORT_ALARMINGPROTOCOL, START_TTL, 0);
      BRNPacketAnno::set_ether_anno(p_out, *_nodeid->getMasterAddress(), brn_etheraddress_broadcast, ETHERTYPE_BRN);
      p_out->timestamp_anno() = Timestamp::now() + Timestamp(0, click_random() % 5);

      send_packets++;

      output(0).push(p_out);
    }
  }

  alarm_types.clear();

  if ( send_packets > 0 ) {
    _timer.reschedule_after_msec(_retransmit_time);
  } else {
    _alarm_active = false;
  }

}


//-----------------------------------------------------------------------------
// Handler
//--------------------------->---------------------------------------------------

enum {H_RETRANSMIT_ACTIVE};

static int
AlarmingRetransmit_write_param(const String &in_s, Element *e, void *vparam,
                             ErrorHandler *errh)
{
  AlarmingRetransmit *as = (AlarmingRetransmit *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_RETRANSMIT_ACTIVE: {    //debug
      bool ret_act;
      if (!cp_bool(s, &ret_act))
        return errh->error("retransmit_active parameter must be bool");
      as->retransmit_active(ret_act);
      break;
    }
  }

  return 0;
}

void
AlarmingRetransmit::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("active", AlarmingRetransmit_write_param,(void *) H_RETRANSMIT_ACTIVE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AlarmingRetransmit)
