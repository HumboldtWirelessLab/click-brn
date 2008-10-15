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
 * tostations.{cc,hh}
 * A. Zubow
 */

#include <click/config.h>
#include "common.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "tostations.hh"

CLICK_DECLS

ToStations::ToStations()
  : _debug(BrnLogger::DEFAULT)
{
}

ToStations::~ToStations()
{
}

int
ToStations::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpElement, "AssocList", &_stations,
		  cpEnd) < 0)
    return -1;

  if (!_stations || !_stations->cast("AssocList")) 
    return errh->error("AssocList not specified");

  return 0;

}

int
ToStations::initialize(ErrorHandler *)
{
  return 0;
}

void
ToStations::push(int, Packet *p_in)
{
  BRN_DEBUG(" * push called()");

  // use ether annotation
  click_ether *ether = (click_ether *) p_in->ether_header();

  if (!ether)
  {
    BRN_DEBUG(" no ether header found -> killing packet.");
    p_in->kill();
    return;
  }


  // get src and dst mac 
  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  if (_stations->is_associated(dst_addr) 
    || dst_addr.is_broadcast()) {
    // packet is associated station
    BRN_DEBUG(" * packet is for me; process.");
    output(0).push(p_in);
    return;
  }

  // packet is not for associated station
  output(1).push(p_in);

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  ToStations *on = (ToStations *)e;
  return String(on->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  ToStations *on = (ToStations *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  on->_debug = debug;
  return 0;
}

void
ToStations::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ToStations)
