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

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"

#include "flooding_piggyback.hh"

CLICK_DECLS

FloodingPiggyback::FloodingPiggyback()
{
  BRNElement::init();
}

FloodingPiggyback::~FloodingPiggyback()
{
}

int
FloodingPiggyback::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FLOODING", cpkP+cpkM, cpElement, &_flooding,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
FloodingPiggyback::simple_action(Packet *p)
{

  return p;
}

int
FloodingPiggyback::bcast_header_add_last_nodes(Flooding *fl, EtherAddress *src, uint32_t id, uint8_t *buffer, uint32_t buffer_size, uint32_t max_last_nodes )
{
  int32_t last_node_cnt = 0;
  Flooding::BroadcastNode *bcn = fl->get_broadcast_node(src);
  if ( bcn == NULL ) return 0;
  
  struct Flooding::BroadcastNode::flooding_last_node* lnl = bcn->get_last_nodes(id, (uint32_t*)&last_node_cnt);
  uint32_t cnt = MIN(last_node_cnt,MIN((buffer_size-2)/6,max_last_nodes));
  
  if ( cnt == 0 ) return 0;
  
  last_node_cnt--; //from count to index

  //take new nodes from the end of the lastnode list
  uint32_t buf_idx = sizeof(struct click_brn_bcast_extra_data);

  for ( uint32_t i = 0; (i < cnt) && (last_node_cnt >= 0); last_node_cnt--) {
    EtherAddress add_node = EtherAddress(lnl[last_node_cnt].etheraddr);
    if ( (add_node != *src) && (!fl->is_local_addr(&add_node)) ) {
      click_chatter("Add lastnode: %s",add_node.unparse().c_str());
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
    click_chatter("BCastNode is unknown. Discard extra info (bcastheader).");
    return 0;
  }

  for(uint32_t i = 0; (i < rx_data_size);) {
    
    struct click_brn_bcast_extra_data *extdat = (struct click_brn_bcast_extra_data *)&(rxdata[i]);

    click_chatter("i: %d Type: %d Size: %d FullSize: %d",i, (uint32_t)extdat->type, (uint32_t)extdat->size,rx_data_size);
    
    if ( extdat->type == BCAST_EXTRA_DATA_LASTNODE ) {
      click_chatter("Found Lastnode stuff: Size: %d",extdat->size);
  
      uint32_t rxdata_idx = i + sizeof(struct click_brn_bcast_extra_data);
      uint32_t last_node_data_idx = sizeof(struct click_brn_bcast_extra_data);
      
      int new_last_node = 0;
      
      for(;last_node_data_idx < extdat->size; last_node_data_idx += 6, rxdata_idx += 6 ) {
        click_chatter("get lastnode: %s",EtherAddress(&(rxdata[rxdata_idx])).unparse().c_str());
        ea = EtherAddress(&(rxdata[rxdata_idx]));
        if (!fl->is_local_addr(&ea)) {              //do not insert myself as lastnode
          if ( bcn->add_last_node(id, &ea, false) != 0 )
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
