/* Copyright (C) 2008 Ulf Hermann
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
 */

#include <click/config.h>
#include <elements/brn/common.hh>
#include "netcoding.h"
#include "netcodingprint.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

CLICK_DECLS

int
NetcodingPrint::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _debug = BrnLogger::DEBUG;
  if (cp_va_kparse(conf, this, errh,
		  cpOptional,
		  cpString, "label", &_label,
		  cpEnd) < 0)
    return -1;
  return 0;
}

/* displays the content of a netcoding packet */
Packet *
NetcodingPrint::simple_action(Packet *p_in)
{
  print(p_in);
  return p_in;
}

void
NetcodingPrint::print(Packet *p_in)
{
  StringAccum sa;

  if (sa.out_of_memory()) {
    click_chatter("no memory for NetcodingPrint");
    return;
  }

  if (_label)
    sa << _label << ": ";

  const click_brn *brn = (const click_brn *)p_in->data();

  if (brn) {
    sa << "dst_port: " << (uint16_t)brn->dst_port << " ";
    sa << "src_port: " << (uint16_t)brn->src_port << " ";
    sa << "body_len: " << htons(brn->body_length) << " ";
    sa << "ttl: " << (uint16_t)brn->ttl << " ";
    sa << "tos: " << (uint16_t)brn->tos << " ";

    
    const click_brn_dsr *brn_dsr
      = (const click_brn_dsr *)(p_in->data() + sizeof(click_brn));

    sa << "dsr_type: " << (uint16_t)brn_dsr->dsr_type << " ";

    EtherAddress dsr_dst(brn_dsr->dsr_dst.data);
    sa << "dsr_dst: " << dsr_dst << " ";
    EtherAddress dsr_src(brn_dsr->dsr_src.data);
    sa << "dsr_src: " << dsr_src << " ";

    sa << "segsleft: " << (uint16_t)brn_dsr->dsr_segsleft << " ";
    sa << "hops: " << (uint16_t)brn_dsr->dsr_hop_count << "(";

    for (int i = 0; i < brn_dsr->dsr_hop_count; i++) {
      EtherAddress hop(brn_dsr->addr[i].hw.data);
      sa << "(" << hop << "," << ntohs(brn_dsr->addr[i].metric) << ")";
    }
    sa << ") ";
    sa << "dsr_salvage: " << (uint16_t)brn_dsr->body.src.dsr_salvage << " ";
  
  	const click_brn_netcoding * nc 
  	= (const click_brn_netcoding *)(p_in->data() + sizeof(click_brn) + sizeof(click_brn_dsr));
  	sa << "batch_id: " << nc->batch_id << " ";
  	sa << "last_fragments_in_packet: ";
  	for (unsigned i = 0; i < sizeof(nc->last_fragments) * 8; ++i) {
  		if (nc->isLastFragment(i))
  			sa << i <<" ";
  	}
  	sa << " ";
  	sa << "fragments_in_batch: " << (unsigned)nc->fragments_in_batch << " ( ";
  	for (unsigned i = 0; i < nc->fragments_in_batch; ++i)
  		sa << (unsigned)nc->multipliers[i] << " ";
  	sa << " )";
  } else {
    sa << "empty";
  }

  BRN_DEBUG("%s", sa.c_str());
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NetcodingPrint)
