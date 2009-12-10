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
 * gps.{cc,hh}
 */

#include <click/config.h>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "gps.hh"

CLICK_DECLS

GPS::GPS()
  :_debug(BrnLogger::DEFAULT)
{
}

GPS::~GPS()
{
}

int
GPS::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
   gi
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
GPS::initialize(ErrorHandler *)
{
  return 0;
}

void
GPS::push( int /*port*/, Packet *packet )
{
  packet->kill();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_position_param(Element *e, void *)
{
  StringAccum sa;
  GPS *gps = (GPS *)e;

  sa << "\n";

  return sa.take_string();
}

static String
read_debug_param(Element *e, void *)
{
  GPS *gps = (GPS *)e;
  return String(gps->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  GPS *gps = (GPS *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  gps->_debug = debug;
  return 0;
}

void
GPS::add_handlers()
{
  add_read_handler("position", read_position_param, 0);
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GPS)
