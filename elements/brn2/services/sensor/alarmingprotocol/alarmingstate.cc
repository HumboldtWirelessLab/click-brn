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
 * AlarmingState.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "alarmingstate.hh"
#include "alarmingprotocol.hh"

CLICK_DECLS

AlarmingState::AlarmingState():
  _hop_limit(DEFAULT_HOP_LIMT),
  _lt(NULL),
  _forward_flags(UPDATE_ALARM_NEW_NODE|UPDATE_ALARM_NEW_ID),
  _retry_limit(DEFAULT_RETRY_LIMIT),
  _min_neighbour_fraction(DEFAULT_MIN_NEIGHBOUT_FRACTION)
{
  BRNElement::init();
}

AlarmingState::~AlarmingState()
{
}

int
AlarmingState::configure(Vector<String> &conf, ErrorHandler* errh)
{
  bool _low_hop_forward = false;

  if (cp_va_kparse(conf, this, errh,
      "LINKTABLE", cpkP, cpElement, &_lt,
      "HOPLIMIT", cpkP, cpInteger, &_hop_limit,
      "LOWHOPFWD", cpkP, cpBool, &_low_hop_forward,
      "RETRIES", cpkP, cpInteger, &_retry_limit,
      "MINNEIGHBOURFRACT", cpkP, cpInteger, &_min_neighbour_fraction,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  if (_low_hop_forward) _forward_flags |= UPDATE_ALARM_UPDATE_HOPS;

  return 0;
}

int
AlarmingState::initialize(ErrorHandler *)
{
  return 0;
}

AlarmingState::AlarmNode *
AlarmingState::get_node_by_address(uint8_t type, const EtherAddress *ea)
{
  for( int i = 0; i < _alarm_nodes.size(); i++ )
    if ( (_alarm_nodes[i]._ea == *ea) && ( _alarm_nodes[i]._type == type ) ) return &_alarm_nodes[i];

  return NULL;
}

uint32_t
AlarmingState::update_alarm(int type, const EtherAddress *ea, int id, int hops, const EtherAddress *fwd)
{
  uint32_t result = 0;
  AlarmNode *an = get_node_by_address(type, ea);
  if ( an == NULL ) {
     result |= UPDATE_ALARM_NEW_NODE;
    _alarm_nodes.push_back(AlarmNode(type,ea));
    an = get_node_by_address(type,ea);
  }

  AlarmInfo *ai = an->get_info_by_id(id);
  if ( ai == NULL ) {
    result |= UPDATE_ALARM_NEW_ID;
    an->add_alarm_info(id, hops);
    ai = an->get_info_by_id(id);
  } else {
    if ( hops < ai->_hops ) {
      ai->_hops = hops;  //TODO: delete Forarders ??
      result |= UPDATE_ALARM_UPDATE_HOPS;
    }
  }

  ForwarderInfo *fi = ai->get_fwd_by_address(fwd);
  if ( fi == NULL ) {
    ai->add_forwarder(fwd);
    result |= UPDATE_ALARM_NEW_FORWARDER;
  }

  if (_lt) {
    fi = ai->get_fwd_by_address(ea);
    if ( fi == NULL ) {
      Vector<EtherAddress> neighbors;
      _lt->get_local_neighbors(neighbors);

      int n;
      for(n = 0; n < neighbors.size(); n++) {
        if ( neighbors[n] == *ea ) {
          ai->add_forwarder(ea);
          result |= UPDATE_ALARM_NEW_FORWARDER;
          break;
        }
      }

      neighbors.clear();
    }
  }

  return result;
}

void
AlarmingState::update_neighbours()
{
  Vector<EtherAddress> neighbors;
  int fwd_i;

  if (_lt) {
    _lt->get_local_neighbors(neighbors);

    if ( neighbors.size() == 0 ) return;

    for( int an_i = 0; an_i < _alarm_nodes.size(); an_i++ ) {
      for( int ai_i = 0; ai_i < _alarm_nodes[an_i]._info.size(); ai_i++ ) {
        AlarmInfo *ai = &(_alarm_nodes[an_i]._info[ai_i]);

        int found_fwd = 0;
        ai->_fwd_missing = false;

        if (ai->_hops < _hop_limit) {
          for( int n_i = 0; n_i < neighbors.size(); n_i++) {
            for( fwd_i = 0; fwd_i < ai->_fwd.size(); fwd_i++ ) {
              ForwarderInfo *fwd = &(ai->_fwd[fwd_i]);
              if ( fwd->_ea == neighbors[n_i] ) {
                found_fwd++;
                break;
              }
            }

            ai->_fract_fwd = (100 * found_fwd) / neighbors.size();
            if ( fwd_i == ai->_fwd.size() ) ai->_fwd_missing = true;
         }
        }
      }
    }
  }
}


void
AlarmingState::get_incomlete_forward_types(int max_fraction, Vector<int> *types)
{
  int type, t_i;
  for( int an_i = _alarm_nodes.size()-1; an_i >= 0; an_i--) {
    type = _alarm_nodes[an_i]._type;
    for( t_i = types->size()-1 ; t_i >= 0; t_i--) {
      int ftype = (*types)[t_i];
//      click_chatter("Want type: %d Found type: %d",type, ftype);
      if ( (*types)[t_i] == type ) break;
    }

    if ( t_i < 0 ) types->push_back(type);
  }

/*for( int i = 0; i < types->size();i++) {
    click_chatter("Type: %d",(*types)[i]);
  }*/
}

void
AlarmingState::get_incomlete_forward_nodes(int max_fraction, int max_retries, int max_hops, int type, Vector<AlarmNode*> *nodes)
{
  for( int an_i = _alarm_nodes.size()-1; an_i >= 0; an_i--) {
    AlarmNode *an = &(_alarm_nodes[an_i]);
    if ( an->_type == type ) {
      for( int ai_i = an->_info.size()-1; (ai_i >= 0); ai_i--) {
        AlarmInfo *ai = &(an->_info[ai_i]);
        if ( (ai->_fract_fwd < max_fraction) && (ai->_retries < max_retries) && ( ai->_hops < _hop_limit ) ) {
          nodes->push_back(an);
          break;
        }
      }
    }
  }
/*click_chatter("Node for type %d:",type);
  for (int i = 0; i < nodes->size(); i++ ) {
    AlarmNode *an = (*nodes)[i];
    click_chatter("Node %s",an->_ea.unparse().c_str());
  }*/
}


static String
read_state_param(Element *e, void *)
{
  AlarmingState *as = (AlarmingState *)e;
  StringAccum sa;

  as->update_neighbours();

  sa << "Size: " << as->_alarm_nodes.size();
  sa << "\nNodes:\n";
  for( int i = 0; i < as->_alarm_nodes.size(); i++ ) {
    sa << " " << as->_alarm_nodes[i]._ea.unparse().c_str();
    sa << " Hops: " << (int)as->_alarm_nodes[i]._info[0]._hops;
    sa << " No. Alarm: " << as->_alarm_nodes[i]._info.size();
    sa << " Fract. Fwd: " << as->_alarm_nodes[i]._info[0]._fract_fwd;
    sa << " Missing Fwd: " << as->_alarm_nodes[i]._info[0]._fwd_missing << "\n";
  }
  return sa.take_string();
}

void
AlarmingState::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("state", read_state_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AlarmingState)
