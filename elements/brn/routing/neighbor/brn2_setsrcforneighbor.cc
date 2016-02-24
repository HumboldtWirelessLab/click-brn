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
 * .{cc,hh} -- forwards brn packets to the right device
 * 
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "brn2_setsrcforneighbor.hh"

CLICK_DECLS

BRN2SetSrcForNeighbor::BRN2SetSrcForNeighbor():
  _debug(0),
  _link_table(NULL),
  _nblist(NULL),
  _use_anno(false)
{
}

BRN2SetSrcForNeighbor::~BRN2SetSrcForNeighbor()
{
}

int
BRN2SetSrcForNeighbor::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LINKTABLE", cpkP, cpElement, &_link_table,
      "NBLIST", cpkP, cpElement, &_nblist,
      "USEANNO", cpkP, cpBool, &_use_anno,
      cpEnd) < 0)
    return -1;

  if ( _nblist && !_nblist->cast("BRN2NBList"))
    return errh->error("NBLIST is not type of BRN2NBList");

  return 0;
}

int
BRN2SetSrcForNeighbor::initialize(ErrorHandler *)
{
  return 0;
}

void
BRN2SetSrcForNeighbor::uninitialize()
{
  //cleanup
}

void
BRN2SetSrcForNeighbor::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) output(0).push(q);
}

Packet *
BRN2SetSrcForNeighbor::pull(int)
{
  Packet *p = NULL;

  if ((p = input(0).pull()) != NULL) p = smaction(p); //new packet

  return p;
}

/* Processes an incoming (brn-)packet. */
Packet *
BRN2SetSrcForNeighbor::smaction(Packet *p_in)
{
  EtherAddress dst;
  click_ether *ether = NULL;

  WritablePacket *p_out = p_in->uniqueify();

  if ( _use_anno ) dst = BRNPacketAnno::src_ether_anno(p_out);
  else {
    ether = reinterpret_cast<click_ether *>(p_out->data());
    dst = EtherAddress(ether->ether_dhost);
  }

  if ( ! dst.is_broadcast() ) {

    const EtherAddress *src = NULL;

    if ( _nblist )        src = _nblist->getDeviceAddressForNeighbor(&dst);
    else if (_link_table) src = _link_table->get_neighbor(dst);  //return the local src addr for the (best) link

    if ( src == NULL ) {
      output(1).push(p_out);
      p_out = NULL;
    } else {
      if ( _use_anno ) BRNPacketAnno::set_src_ether_anno(p_out, *src);
      else memcpy(ether->ether_shost, src->data(), 6);
    }
  }

  return p_out;

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRN2SetSrcForNeighbor *ds = reinterpret_cast<BRN2SetSrcForNeighbor *>(e);
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRN2SetSrcForNeighbor *ds = reinterpret_cast<BRN2SetSrcForNeighbor *>(e);
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
BRN2SetSrcForNeighbor::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

EXPORT_ELEMENT(BRN2SetSrcForNeighbor)

CLICK_ENDDECLS
