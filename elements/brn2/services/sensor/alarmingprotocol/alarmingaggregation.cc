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
 * AlarmingAggregation.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>

#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "alarmingaggregation.hh"
#include "alarmingstate.hh"
#include "alarmingprotocol.hh"

CLICK_DECLS

AlarmingAggregation::AlarmingAggregation():
  _delay_queue(NULL),
  _alarm_ret(NULL),
  _no_agg(0),
  _no_pkts(0)
{
  BRNElement::init();
}

AlarmingAggregation::~AlarmingAggregation()
{
}

int
AlarmingAggregation::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ALARMINGSTATE", cpkP+cpkM, cpElement, &_as,
      "DELAYQUEUE", cpkP, cpElement, &_delay_queue,
      "ALARMINGRETRANSMIT", cpkP, cpElement, &_alarm_ret,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
AlarmingAggregation::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
AlarmingAggregation::pull(int /*port*/)
{
  bool has_next = true;
  Packet *p, *p_agg;
  Packet *p_out = NULL;

  int p_out_ttl, p_agg_ttl;

  do {
    p = input(0).pull();
    if ( p != NULL ) {
      if ( p_out == NULL ) {     //first packet will used
        p_out_ttl = BRNProtocol::get_brnheader(p)->ttl;
        p_out = BRNProtocol::pull_brn_header(p);
/*      if ( _delay_queue != NULL ) {
          click_chatter("Delayqueue: %d", _delay_queue->size());
        }*/
      } else {
        p_agg_ttl = BRNProtocol::get_brnheader(p)->ttl;
        p_agg = BRNProtocol::pull_brn_header(p);
        int count_info_in_p = AlarmingProtocol::get_count_nodes(p);  //next packet

//        click_chatter("Got %d in packets",count_info_in_p);
        for ( int i = 0; i < count_info_in_p; i++ ) {                //for each data, check whether it is already in the out packet
          struct alarming_node *an_agg = AlarmingProtocol::get_node_struct(p_agg, i);

          EtherAddress n_ea = EtherAddress(an_agg->node_ea);
          struct alarming_node *an_p_out = AlarmingProtocol::get_node_struct(p_out, &n_ea, an_agg->id);

          if ( an_p_out != NULL ) {  //already in, so try update
//            click_chatter("Found node");
            if ((an_p_out->ttl - p_out_ttl) > (an_agg->ttl - p_agg_ttl)) {  //found smaller hop_count, so update
//              click_chatter("Update");
              an_p_out->ttl = p_out_ttl + an_agg->ttl - p_agg_ttl;
            }
          } else {                   //not included, so add info
//            click_chatter("New node");
            int new_ttl = p_out_ttl + (an_agg->ttl - p_agg_ttl);
            if ((new_ttl < 0)||(new_ttl > 255)) new_ttl = 255;              //bugfix. TODO: remove
            p_out = AlarmingProtocol::add_node(p_out, &n_ea, new_ttl, an_agg->id);
          }
        }

        p_agg->kill();

        has_next = ( p_out->length() < 1200 );
      }
    } else {
      has_next = false;
    }
  } while (has_next);

  if ((_delay_queue != NULL) && (p_out != NULL)) {
    int dq_size = _delay_queue->size();

    for( int p_i = (dq_size - 1); p_i >= 0; p_i-- ) {
      p = _delay_queue->get_packet(p_i);

      p_agg_ttl = BRNProtocol::get_brnheader(p)->ttl;
      p_agg = BRNProtocol::pull_brn_header(p);
      int count_info_in_p = AlarmingProtocol::get_count_nodes(p);  //next packet

//      click_chatter("Got %d in packets",count_info_in_p);
      for ( int i = 0; i < count_info_in_p; i++ ) {                //for each data, check whether it is already in the out packet
        struct alarming_node *an_agg = AlarmingProtocol::get_node_struct(p_agg, i);

        EtherAddress n_ea = EtherAddress(an_agg->node_ea);
        struct alarming_node *an_p_out = AlarmingProtocol::get_node_struct(p_out, &n_ea, an_agg->id);

        if ( an_p_out != NULL ) {  //already in, so try update
//          click_chatter("Found node");
          if ((an_p_out->ttl - p_out_ttl) > (an_agg->ttl - p_agg_ttl)) {  //found smaller hop_count, so update
//            click_chatter("Update");
            an_p_out->ttl = p_out_ttl + an_agg->ttl - p_agg_ttl;
          }
        } else {                   //not included, so add info
 //         click_chatter("New node");
          int new_ttl = p_out_ttl + (an_agg->ttl - p_agg_ttl);
          if ((new_ttl < 0)||(new_ttl > 255)) new_ttl = 255;              //bugfix. TODO: remove
          p_out = AlarmingProtocol::add_node(p_out, &n_ea, new_ttl, an_agg->id);
        }
      }

      _delay_queue->remove_packet(p_i);

    }
  }

  if ( p_out != NULL ) {
    p_out = BRNProtocol::add_brn_header(p_out, BRN_PORT_ALARMINGPROTOCOL, BRN_PORT_ALARMINGPROTOCOL, p_out_ttl, 0);
    _no_pkts++;
  } else {
    if ( _no_pkts > 0 && _alarm_ret != NULL ) {
      _alarm_ret->retransmit_active(true);
    }
  }
  return p_out;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
AlarmingAggregation::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AlarmingAggregation)
