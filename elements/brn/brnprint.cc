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
 * brnprint.{cc,hh} -- prints out the content of a brn packet
 * A. Zubow
 */

#include <click/config.h>
#include "common.hh"

#include "brnprint.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
CLICK_DECLS

BRNPrint::BRNPrint()
  : _active( true )
{
}

BRNPrint::~BRNPrint()
{
}

int
BRNPrint::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "label", &_label,
		  cpEnd) < 0)
    return -1;
  return 0;
}

int
BRNPrint::initialize(ErrorHandler *)
{
  return 0;
}

/* displays the content of a brn packet */
Packet *
BRNPrint::simple_action(Packet *p_in)
{
  if( false == _active )
    return( p_in );
    
  print(p_in);
  return p_in;
}

void
BRNPrint::print(Packet *p_in)
{
  StringAccum sa;

  if (sa.out_of_memory()) {
    click_chatter("no memory for BRNPrint");
    return;
  }
/*
  if (_label)
    sa << _label << ": ";
*/
  const click_brn *brn = (const click_brn *)p_in->data();

  if (brn) {
    sa << "dst_port: " << (uint16_t)brn->dst_port << " ";
    sa << "src_port: " << (uint16_t)brn->src_port << " ";
    sa << "body_len: " << htons(brn->body_length) << " ";
    sa << "ttl: " << (uint16_t)brn->ttl << " ";
    sa << "tos: " << (uint16_t)brn->tos << " ";

    if (brn->dst_port == BRN_PORT_DSR) 
    {
      sa << "(dsr) ";
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
        sa << "(" << hop << "," << (uint16_t)brn_dsr->addr[i].metric << ")";
      }
      sa << ") ";

      if (brn_dsr->dsr_type == BRN_DSR_SRC) {
        sa << "(dsr_src) ";
        sa << "dsr_salvage: " << (uint16_t)brn_dsr->body.src.dsr_salvage << " ";
      } else if (brn_dsr->dsr_type == BRN_DSR_RREQ) {
        sa << "(dsr_rreq) ";
        sa <<  "dsr_id: " << htons(brn_dsr->body.rreq.dsr_id) << " ";
      } else if (brn_dsr->dsr_type == BRN_DSR_RREP) {
        sa << "(dsr_rrep) ";
        sa <<  "dsr_flags: " << (uint16_t)brn_dsr->body.rrep.dsr_flags << " ";
        sa <<  ",dsr_pad: " << (uint16_t)brn_dsr->body.rrep.dsr_pad << " ";
      } else if (brn_dsr->dsr_type == BRN_DSR_RERR) {
        sa << "(dsr_rerr) ";

        EtherAddress dsr_unreachable_src(brn_dsr->body.rerr.dsr_unreachable_src.data);
        sa << "dsr_unreachable_src: " << dsr_unreachable_src << " ";
        EtherAddress dsr_unreachable_dst(brn_dsr->body.rerr.dsr_unreachable_dst.data);
        sa << "dsr_unreachable_dst: " << dsr_unreachable_dst << " ";

        EtherAddress dsr_msg_originator(brn_dsr->body.rerr.dsr_msg_originator.data);
        sa << "dsr_msg_originator: " << dsr_msg_originator << " ";

        sa << "dsr_flags: " << (uint16_t)brn_dsr->body.rerr.dsr_flags << " ";
        sa << "dsr_error: " << (uint16_t)brn_dsr->body.rerr.dsr_error << " ";
      }
    } 
    else if (brn->dst_port == BRN_PORT_BCAST) 
    {
      sa << "(broadcast) ";
      const click_brn_bcast *brn_bcast =
          (click_brn_bcast *)(p_in->data() + sizeof(click_ether));
      sa << "bcast_id: %d\n" << htons(brn_bcast->bcast_id) << " ";

      EtherAddress dsr_dst(brn_bcast->dsr_dst.data);
      sa << "dsr_dst: " << dsr_dst << " ";
      EtherAddress dsr_src(brn_bcast->dsr_src.data);
      sa << "dsr_src: " << dsr_src << " ";
    }
    else if (brn->dst_port == BRN_PORT_DHT)
    {
    }
  } else {
    sa << "empty";
  }

  click_chatter("%s", sa.c_str());
}

static String
read_handler(Element *, void *)
{
  return "false\n";
}

static String
read_active_param(Element *e, void *)
{
  BRNPrint *rq = (BRNPrint *)e;
  return String(rq->_active) + "\n";
}

static int 
write_active_param(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  BRNPrint *rq = (BRNPrint *)e;
  String s = cp_uncomment(in_s);
  bool active;
  if (!cp_bool(s, &active)) 
    return errh->error("active parameter must be boolean");
  rq->_active = active;

  return 0;
}

void
BRNPrint::add_handlers()
{
  add_read_handler("active", read_active_param, 0);
  add_write_handler("active", write_active_param, 0);

  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNPrint)
