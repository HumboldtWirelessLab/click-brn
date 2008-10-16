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
 * fromdevsim.{cc,hh} -- simulates a click fromdevice element (for debugging purpose).
 * A. Zubow
 */

#include <click/config.h>
#include "elements/brn/common.hh"
#include "fromdevsim.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
CLICK_DECLS

FromDevSim::FromDevSim():
  _debug(BrnLogger::DEFAULT)
{
}

FromDevSim::~FromDevSim()
{
}

int
FromDevSim::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "device name", &_dev_name,
		  cpEnd) < 0)
    return -1;

  return 0;
}

int
FromDevSim::initialize(ErrorHandler *)
{
  return 0;
}

/* displays the content of a brn packet */
Packet *
FromDevSim::simple_action(Packet *p_in)
{
  // set device anno
 // p_in->set_udevice_anno(_dev_name.c_str());

  return p_in;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  FromDevSim *ds = (FromDevSim *)e;
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  FromDevSim *ds = (FromDevSim *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
FromDevSim::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FromDevSim)
