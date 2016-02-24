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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "brn2_toclient.hh"

CLICK_DECLS

BRN2ToStations::BRN2ToStations()
  : _debug(BrnLogger::DEFAULT),
    _stations(NULL){
}

BRN2ToStations::~BRN2ToStations()
{
}

int
BRN2ToStations::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ASSOCLIST", cpkP+cpkM, cpElement, &_stations,
    cpEnd) < 0)
    return -1;

  if (!_stations || !_stations->cast("BRN2AssocList")) 
    return errh->error("AssocList not specified");

  return 0;

}

int
BRN2ToStations::initialize(ErrorHandler *)
{
  return 0;
}

void
BRN2ToStations::push(int, Packet *p_in)
{
  BRN_DEBUG(" * push called()");

  // use ether annotation
  const click_ether *ether = reinterpret_cast<const click_ether *>( p_in->ether_header());

  if (!ether)
  {
    BRN_DEBUG(" no ether header found -> killing packet.");
    p_in->kill();
    return;
  }


  // get src and dst mac 
  EtherAddress dst_addr(ether->ether_dhost);

  if (_stations->is_associated(dst_addr) ) {
    output(0).push(p_in);
    return;
  } else if (dst_addr.is_broadcast()) {
    output(1).push(p_in);
    return;
  }

  // packet is not for associated station
  output(2).push(p_in);

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRN2ToStations *on = reinterpret_cast<BRN2ToStations *>(e);
  return String(on->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRN2ToStations *on = reinterpret_cast<BRN2ToStations *>(e);
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  on->_debug = debug;
  return 0;
}

void
BRN2ToStations::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2ToStations)
