/* Copyright (C) 2005 BerlifnRoofNet Lab
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
#include <clicknet/wifi.h>

#include "elements/brn/brn2.h"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "flooding_db.hh"
#include "../../identity/brn2_nodeidentity.hh"

CLICK_DECLS

FloodingDB::FloodingDB()
{
  BRNElement::init();
}

FloodingDB::~FloodingDB()
{
  reset();
}

int
FloodingDB::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
FloodingDB::initialize(ErrorHandler *)
{
  return 0;
}


BroadcastNode*
FloodingDB::add_broadcast_node(EtherAddress *src)
{
  BroadcastNode* bcn = _bcast_map.find(*src);
  if ( bcn == NULL ) {
    bcn = new BroadcastNode(src);
    _bcast_map.insert(*src, bcn);
  }

  return bcn;
}

BroadcastNode*
FloodingDB::get_broadcast_node(EtherAddress *src)
{
  BroadcastNode** bnp = _bcast_map.findp(*src);
  if ( bnp == NULL ) {
    BRN_DEBUG("Couldn't find %s",src->unparse().c_str());
    return NULL;
  }
  return *bnp;
}

void
FloodingDB::add_id(EtherAddress *src, uint16_t id, Timestamp *now, bool me_src)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    BRN_DEBUG("Add BCASTNODE");
    bcn = new BroadcastNode(src);
    _bcast_map.insert(*src, bcn);
  }

  bcn->add_id(id, *now, me_src);
}

int
FloodingDB::add_node_info(EtherAddress *src, uint16_t id, EtherAddress *last_node, bool forwarded, bool rx_acked, bool responsibility, bool foreign_responsibility)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    BRN_ERROR("BCastNode is unknown. Discard info.");
    return -1;
  }

  return bcn->add_node_info(id, last_node, forwarded, rx_acked, responsibility, foreign_responsibility);
}

int
FloodingDB::add_responsible_node(EtherAddress *src, uint16_t id, EtherAddress *responsible_node)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    BRN_ERROR("BCastNode is unknown. Discard info.");
    return -1;
  }

  return bcn->add_responsible_node(id, responsible_node);
}

void
FloodingDB::inc_received(EtherAddress *src, uint16_t id, EtherAddress *last_node)
{
  uint32_t *cnt = _recv_cnt.findp(*last_node);
  if ( cnt == NULL ) _recv_cnt.insert(*last_node,1);
  else (*cnt)++;

  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    BRN_ERROR("BCastNode is unknown. Discard info.");
    return;
  }

  bcn->add_recv_last_node(id, last_node);
}

bool
FloodingDB::have_id(EtherAddress *src, uint16_t id, Timestamp *now, uint32_t *count_forwards)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    *count_forwards = 0;
    return false;
  }

  *count_forwards = bcn->forward_attempts(id);

  return bcn->have_id(id,*now);
}

void
FloodingDB::forward_attempt(EtherAddress *src, uint16_t id)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return;

  bcn->forward_attempt(id);
}

void
FloodingDB::forward_done(EtherAddress *src, uint16_t id, bool success)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return;

  bcn->forward_done(id, success);
}

uint32_t
FloodingDB::forward_done_cnt(EtherAddress *src, uint16_t id)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return 0;

  return bcn->forward_done_cnt(id);
}

uint32_t
FloodingDB::unfinished_forward_attempts(EtherAddress *src, uint16_t id)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return 0;

  return bcn->unfinished_forward_attempts(id);
}

void
FloodingDB::sent(EtherAddress *src, uint16_t id, uint32_t no_transmission, uint32_t no_rts_transmissions)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return;

  bcn->sent(id, no_transmission, no_rts_transmissions);
}

bool
FloodingDB::me_src(EtherAddress *src, uint16_t id)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return false;

  return bcn->me_src(id);
}

void
FloodingDB::reset()
{
  if ( _bcast_map.size() > 0 ) {
    BcastNodeMapIter iter = _bcast_map.begin();
    while (iter != _bcast_map.end())
    {
      BroadcastNode* bcn = iter.value();
      delete bcn;
      iter++;
    }

    _bcast_map.clear();
  }
}

struct BroadcastNode::flooding_node_info*
FloodingDB::get_node_info(EtherAddress *src, uint16_t id, EtherAddress *last)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn == NULL ) return NULL;
  return bcn->get_node_info(id, last);
}

struct BroadcastNode::flooding_node_info*
FloodingDB::get_node_infos(EtherAddress *src, uint16_t id, uint32_t *size)
{
  *size = 0;
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) return bcn->get_node_infos(id, size);
  return NULL;
}

void
FloodingDB::assign_last_node(EtherAddress *src, uint16_t id, EtherAddress *last_node)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return;

  int res = bcn->assign_node(id, last_node);
  BRN_DEBUG("Finished assign node: %s %d",last_node->unparse().c_str(),res);
}

void
FloodingDB::clear_assigned_nodes(EtherAddress *src, uint16_t id)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) return bcn->clear_assigned_nodes(id);  
}

void
FloodingDB::set_responsibility_target(EtherAddress *src, uint16_t id, EtherAddress *target)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) bcn->set_responsibility_target(id, target);
}

void
FloodingDB::set_foreign_responsibility_target(EtherAddress *src, uint16_t id, EtherAddress *target)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) bcn->set_foreign_responsibility_target(id, target);
}

bool
FloodingDB::is_responsibility_target(EtherAddress *src, uint16_t id, EtherAddress *target)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) return bcn->is_responsibility_target(id, target);
  return false;
}

String
FloodingDB::table()
{
  StringAccum sa;

  sa << "<flooding_table node=\"" << BRN_NODE_NAME << "\">\n";
  BcastNodeMapIter iter = _bcast_map.begin();
  while (iter != _bcast_map.end())
  {
    BroadcastNode* bcn = iter.value();
    int id_c = 0;
    for( uint32_t i = 0; i < DEFAULT_MAX_BCAST_ID_QUEUE_SIZE; i++ )
       if ( bcn->_bcast_id_list[i] != 0 ) id_c++;

    sa << "\t<src node=\"" << bcn->_src.unparse() << "\" id_count=\"" << id_c << "\">\n";
    for( uint32_t i = 0; i < DEFAULT_MAX_BCAST_ID_QUEUE_SIZE; i++ ) {
      if ( bcn->_bcast_id_list[i] == 0 ) continue; //unused id-entry
      struct BroadcastNode::flooding_node_info *flnl = bcn->_flooding_node_info_list[i];
      sa << "\t\t<id value=\"" << bcn->_bcast_id_list[i] << "\" fwd=\"";
      sa << (uint32_t)bcn->_bcast_fwd_list[i] << "\" fwd_done=\"";
      sa << (uint32_t)bcn->_bcast_fwd_done_list[i] << "\" fwd_succ=\"";
      sa << (uint32_t)bcn->_bcast_fwd_succ_list[i] <<	"\" sent=\"";
      sa << (uint32_t)bcn->_bcast_snd_list[i] << "\" rts_sent=\"";
      sa << (uint32_t)bcn->_bcast_rts_snd_list[i] << "\" time=\"";
      sa << bcn->_bcast_time_list[i].unparse() << "\" unicast_target=\"";
      sa << (uint32_t)(((bcn->_bcast_flags_list[i] & FLOODING_FLAGS_ME_UNICAST_TARGET) == 0)?0:1) << "\" >\n";

      for ( int j = 0; j < bcn->_flooding_node_info_list_size[i]; j++ ) {
        sa << "\t\t\t<lastnode addr=\"" << EtherAddress(flnl[j].etheraddr).unparse();
        sa << "\" forwarded=\"" << (uint32_t)(flnl[j].flags & FLOODING_NODE_INFO_FLAGS_FORWARDED);
        sa << "\" responsible=\"" << (uint32_t)(((flnl[j].flags & FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY) == 0)?0:1);
        sa << "\" finished_responsible=\"" << (uint32_t)(((flnl[j].flags & FLOODING_NODE_INFO_FLAGS_FINISHED_RESPONSIBILITY) == 0)?0:1);
        sa << "\" foreign_responsible=\"" << (uint32_t)(((flnl[j].flags & FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY) == 0)?0:1);
        sa << "\" guess_foreign_responsible=\"" << (uint32_t)(((flnl[j].flags & FLOODING_NODE_INFO_FLAGS_GUESS_FOREIGN_RESPONSIBILITY) == 0)?0:1);
        sa << "\" rx_acked=\"" << (uint32_t)(((flnl[j].flags & FLOODING_NODE_INFO_FLAGS_FINISHED) == 0)?0:1);
        sa << "\" rcv_cnt=\"" << (uint32_t)(flnl[j].received_cnt);
        sa << "\" unicast_target=\"" << (uint32_t)(((flnl[j].flags & FLOODING_NODE_INFO_FLAGS_NODE_WAS_UNICAST_TARGET) == 0)?0:1) << "\" />\n";
      }

      sa << "\t\t</id>\n";

    }
    sa << "\t</src>\n";
    iter++;
  }
  sa << "</flooding_table>\n";

  return sa.take_string();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_table_param(Element *e, void *)
{
  return ((FloodingDB *)e)->table();
}

static int 
write_reset_param(const String &/*in_s*/, Element *e, void */*vparam*/, ErrorHandler */*errh*/)
{
  ((FloodingDB *)e)->reset();

  return 0;
}

void
FloodingDB::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("forward_table", read_table_param, 0);

  add_write_handler("reset", write_reset_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingDB)
