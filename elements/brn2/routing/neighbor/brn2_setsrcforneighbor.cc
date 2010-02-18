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

#include "brn2_setsrcforneighbor.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

CLICK_DECLS

BRN2SetSrcForNeighbor::BRN2SetSrcForNeighbor()
{
}

BRN2SetSrcForNeighbor::~BRN2SetSrcForNeighbor()
{
}

int
BRN2SetSrcForNeighbor::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NBLIST", cpkP+cpkM, cpElement, &_nblist,
      cpEnd) < 0)
    return -1;

  if (!_nblist || !_nblist->cast("BRN2NBList"))
    return errh->error("BRN2NBList not specified");

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

/* Processes an incoming (brn-)packet. */
void
BRN2SetSrcForNeighbor::push(int /*port*/, Packet *p_in)
{
  EtherAddress ea;
  const EtherAddress *src;
  //int outputport = -1;

  click_ether *ether = (click_ether *) p_in->data();

  ea = EtherAddress(ether->ether_dhost);

  if ( _nblist->isContained(&ea) ) {
    src = _nblist->getDeviceAddressForNeighbor(&ea);
    if ( src != NULL ) {
      memcpy(ether->ether_shost, src->data(), 6);
      output(0).push(p_in);
      return;
    }
  }

  output(1).push(p_in);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRN2SetSrcForNeighbor *ds = (BRN2SetSrcForNeighbor *)e;
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRN2SetSrcForNeighbor *ds = (BRN2SetSrcForNeighbor *)e;
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
