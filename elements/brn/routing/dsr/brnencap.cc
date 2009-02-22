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
#include "elements/brn/common.hh"

#include "brnencap.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
CLICK_DECLS

BRNEncap::BRNEncap()
  : _debug(BrnLogger::DEFAULT)
{
}

BRNEncap::~BRNEncap()
{
}

int
BRNEncap::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

int
BRNEncap::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
BRNEncap::add_brn_header(Packet *p)
{
  // there's not really much mention of TTL in the IETF draft (other
  // than in the case of RREQs), I suppose it's sort of implicitly the
  // length of the source route.  so right now we're not checking OR
  // decrementing the TTL when forwarding (again, except in the case
  // of RREQs).
  //
  return add_brn_header(p, 255); // TODO use constants
}

Packet *
BRNEncap::add_brn_header(Packet *p_in, unsigned int ttl)
{
  int payload = sizeof(click_brn);

  BRN_DEBUG(" * creating BRN packet with payload %d\n", payload);

  // add the extra header size and get a new packet
  WritablePacket *p = p_in->push(payload);
  if (!p) {
    BRN_FATAL("couldn't add space for new DSR header\n");
    return p;
  }

  // set brn header
  click_brn *brn = (click_brn *)p_in->data();
  brn->dst_port = BRN_PORT_DSR;
  brn->src_port = BRN_PORT_DSR;

  brn->ttl = ttl;
  brn->tos = BRNPacketAnno::tos_anno(p_in);

  return p;
}

static String
read_debug_param(Element *e, void *)
{
  BRNEncap *be = (BRNEncap *)e;
  return String(be->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRNEncap *be = (BRNEncap *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  be->_debug = debug;
  return 0;
}

void
BRNEncap::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNEncap)
