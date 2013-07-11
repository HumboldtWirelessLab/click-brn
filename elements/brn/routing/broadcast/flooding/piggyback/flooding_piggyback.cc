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
 * brnetherdecap.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"

#include "flooding_piggyback.hh"


CLICK_DECLS

FloodingPiggyback::FloodingPiggyback():
  _me(NULL),
  _flooding(NULL),
  _fhelper(NULL),
  _max_last_nodes_per_pkt(BCAST_EXTRA_DATA_LASTNODE_DFL_MAX_NODES),
  _update_interval(BCAST_EXTRA_DATA_NEIGHBOURS_UPDATE_INTERVAL)
{
  BRNElement::init();
}

FloodingPiggyback::~FloodingPiggyback()
{
  _fhelper->clear_graph(_net_graph);
}

int
FloodingPiggyback::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "FLOODING", cpkP+cpkM, cpElement, &_flooding,
      "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
      "LASTNODESPERPKT", cpkP, cpInteger, &_max_last_nodes_per_pkt,
      "NEIGHBOURSUPDATEINTERVAL", cpkP, cpInteger, &_update_interval,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  _last_update = Timestamp::now();
  _fhelper->clear_graph(_net_graph);

  return 0;
}

Packet *
FloodingPiggyback::simple_action(Packet *p)
{
  if ( _max_last_nodes_per_pkt > 0 ) {
    
    if ( (p->timestamp_anno() - _last_update).msecval() > _update_interval ) {
      _last_update = p->timestamp_anno();
      _fhelper->clear_graph(_net_graph);
      _fhelper->init_graph(*(_me->getMasterAddress()), _net_graph, 100);
      _fhelper->get_graph(_net_graph, 2, 100);
    }

    struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)&(p->data()[sizeof(struct click_brn)]);
    uint16_t bcast_id = ntohs(bcast_header->bcast_id);
    uint32_t rxdatasize =  bcast_header->extra_data_size;

    EtherAddress src = EtherAddress((uint8_t*)&(p->data()[sizeof(struct click_brn) + sizeof(struct click_brn_bcast) + rxdatasize]));

    //Copy Header
    uint8_t header_cpy[sizeof(struct click_brn) + sizeof(struct click_brn_bcast)];
    memcpy(header_cpy, p->data(), sizeof(struct click_brn) + sizeof(struct click_brn_bcast));

    //Check extra data
    uint8_t *rxdata = (uint8_t*)&(bcast_header[1]);
    uint8_t last_node_data_size = 0;
    uint32_t last_node_data_index = 0;

    for (uint32_t i = 0; (i < bcast_header->extra_data_size);) {

      struct click_brn_bcast_extra_data *extdat = (struct click_brn_bcast_extra_data *)&(rxdata[i]);

      if ( extdat->type == BCAST_EXTRA_DATA_LASTNODE ) {
        BRN_DEBUG("Found Lastnode stuff: Size: %d",extdat->size);
        last_node_data_size = extdat->size;
        last_node_data_index = i;
        break; 
      } else i += extdat->size;
    }

    if ( last_node_data_size == 0 ) {                             //no last node data
      extra_data_size = bcast_header->extra_data_size;
      memcpy( extra_data, rxdata, bcast_header->extra_data_size);
    } else {                                                     //found last node data
      extra_data_size = 0;
      //Copy parts before
      if ( last_node_data_index > 0 ) {
        memcpy( extra_data, rxdata, last_node_data_index);
        extra_data_size += last_node_data_index;
      }
 
      //Copy parts after
      if ( bcast_header->extra_data_size > (last_node_data_index + last_node_data_size) ) {
        memcpy( &(extra_data[extra_data_size]),
                &(rxdata[last_node_data_index + last_node_data_size]),
                bcast_header->extra_data_size - (last_node_data_index + last_node_data_size) );
        extra_data_size += bcast_header->extra_data_size - (last_node_data_index + last_node_data_size);
      }
    }

    uint32_t new_data_size = FloodingPiggyback::bcast_header_add_last_nodes(_flooding, &src, bcast_id, &(extra_data[extra_data_size]),
                                                                            BCAST_MAX_EXTRA_DATA_SIZE-extra_data_size, _max_last_nodes_per_pkt,
                                                                            _net_graph);

    if ( new_data_size > 0 ) {
      extra_data_size += new_data_size;
    } else if ( bcast_header->extra_data_size == 0 ) { //no new data & no old data,so send org packet
      BRN_WARN("No old an dno new data. Take old one.");
      return p;
    }

    WritablePacket *p_new = p->uniqueify();

    if ( extra_data_size < bcast_header->extra_data_size ) {
      p_new->pull(bcast_header->extra_data_size - extra_data_size);
    } else if ( extra_data_size > bcast_header->extra_data_size ) {
      p_new = p->push(extra_data_size - bcast_header->extra_data_size);
      if ( p_new == NULL ) {
        BRN_ERROR("Couldn't resize packet. Take old one.");
        return p;
      }
    }

    //copy header
    memcpy(p_new->data(), header_cpy, sizeof(struct click_brn) + sizeof(struct click_brn_bcast));

    //copy extra data
    bcast_header = (struct click_brn_bcast *)&(p->data()[sizeof(struct click_brn)]);
    memcpy(&(p_new->data()[sizeof(struct click_brn) + sizeof(struct click_brn_bcast)]), extra_data, extra_data_size);
    bcast_header->extra_data_size = extra_data_size;  //set new size

    return p_new;
  }

  return p;
}

int
FloodingPiggyback::bcast_header_add_last_nodes(Flooding *fl, EtherAddress *src, uint32_t id, uint8_t *buffer, uint32_t buffer_size, uint32_t max_last_nodes, NetworkGraph &net_graph)
{
  int32_t last_node_cnt = 0;
  Flooding::BroadcastNode *bcn = fl->get_broadcast_node(src);
  if ( bcn == NULL ) return 0;

  struct Flooding::BroadcastNode::flooding_last_node* lnl = bcn->get_last_nodes(id, (uint32_t*)&last_node_cnt);
  uint32_t cnt = MIN((uint32_t)last_node_cnt,MIN((buffer_size-2)/6,max_last_nodes));

  if ( cnt == 0 ) return 0;

  last_node_cnt--; //from count to index

  //take new nodes from the end of the lastnode list
  uint32_t buf_idx = sizeof(struct click_brn_bcast_extra_data);

  for ( uint32_t i = 0; (i < cnt) && (last_node_cnt >= 0); last_node_cnt--) {
    EtherAddress add_node = EtherAddress(lnl[last_node_cnt].etheraddr);
    if ( (add_node != *src) && (!fl->is_local_addr(&add_node)) && (net_graph.nmm.findp(add_node) != NULL) ) {
      //click_chatter("Add lastnode: %s",add_node.unparse().c_str());
      memcpy(&(buffer[buf_idx]), lnl[last_node_cnt].etheraddr, 6);
      buf_idx = buf_idx + 6;
      i++;
    }
  }

  if ( buf_idx == sizeof(struct click_brn_bcast_extra_data) ) return 0;

  struct click_brn_bcast_extra_data *extdat = (struct click_brn_bcast_extra_data *)buffer;
  extdat->size = buf_idx;
  extdat->type = BCAST_EXTRA_DATA_LASTNODE;

  return buf_idx;
  return 0;
}

int
FloodingPiggyback::bcast_header_get_last_nodes(Flooding *fl, EtherAddress *src, uint32_t id, uint8_t *rxdata, uint32_t rx_data_size )
{
  EtherAddress ea;

  Flooding::BroadcastNode *bcn = fl->get_broadcast_node(src);
  if ( bcn == NULL ) {
    //click_chatter("BCastNode %s is unknown. Discard extra info (bcastheader).",src->unparse().c_str());
    return 0;
  }

  for(uint32_t i = 0; (i < rx_data_size);) {

    struct click_brn_bcast_extra_data *extdat = (struct click_brn_bcast_extra_data *)&(rxdata[i]);

    //click_chatter("i: %d Type: %d Size: %d FullSize: %d",i, (uint32_t)extdat->type, (uint32_t)extdat->size,rx_data_size);

    if ( extdat->type == BCAST_EXTRA_DATA_LASTNODE ) {
      //click_chatter("Found Lastnode stuff: Size: %d",extdat->size);

      uint32_t rxdata_idx = i + sizeof(struct click_brn_bcast_extra_data);
      uint32_t last_node_data_idx = sizeof(struct click_brn_bcast_extra_data);

      int new_last_node = 0;

      for(;last_node_data_idx < extdat->size; last_node_data_idx += 6, rxdata_idx += 6 ) {
        //click_chatter("get lastnode: %s",EtherAddress(&(rxdata[rxdata_idx])).unparse().c_str());
        ea = EtherAddress(&(rxdata[rxdata_idx]));
        if (!fl->is_local_addr(&ea)) {              //do not insert myself as lastnode
          if ( bcn->add_last_node(id, &ea, false, false) != 0 )
            fl->_flooding_last_node_due_to_piggyback++;
        }
      }
      return new_last_node;
    } else i += extdat->size;
  }

  return 0;
}

void
FloodingPiggyback::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingPiggyback)
