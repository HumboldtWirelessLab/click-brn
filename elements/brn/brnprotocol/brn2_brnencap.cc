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
 * brnencap.{cc,hh} -- prepends a brn header
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "brnprotocol.hh"
#include "brn2_brnencap.hh"

CLICK_DECLS

BRN2Encap::BRN2Encap()
  :   _src_port(0),
      _dst_port(0),
      _ttl(0),
      _tos(0),
      _debug(0)
{
}

BRN2Encap::~BRN2Encap()
{
}

int
BRN2Encap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "SRCPORT", cpkP+cpkM , cpInteger, &_src_port,
      "DSTPORT", cpkP+cpkM , cpInteger, &_dst_port,
      "TTL", cpkP+cpkM , cpInteger, &_ttl,
      "TOS", cpkP+cpkM , cpInteger, &_tos,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
BRN2Encap::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
BRN2Encap::smaction(Packet *p)
{
  WritablePacket *brnp = BRNProtocol::add_brn_header(p, _src_port, _dst_port, _ttl, _tos);
  if ( brnp == NULL ) {
    click_chatter("Error in BRN2Encap");
    return p;
  }

  BRNPacketAnno::dec_pulled_bytes_anno(p, sizeof(click_brn));

  return brnp;
}

void
BRN2Encap::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) {
    output(0).push(q);
  }
}

Packet *
BRN2Encap::pull(int)
{
  if (Packet *p = input(0).pull()) {
    return smaction(p);
  } else {
    return 0;
  }
}

/*****************************************************************************/
/******************************* H A N D L E R *******************************/
/*****************************************************************************/

static String
read_debug_param(Element *e, void *)
{
  BRN2Encap *be = reinterpret_cast<BRN2Encap *>(e);
  return String(be->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BRN2Encap *be = reinterpret_cast<BRN2Encap *>(e);
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  be->_debug = debug;
  return 0;
}

void
BRN2Encap::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Encap)
